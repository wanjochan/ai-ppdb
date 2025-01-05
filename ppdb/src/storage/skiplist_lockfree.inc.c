#include <internal/storage.h>

// 跳表节点结构
typedef struct ppdb_skiplist_node {
    ppdb_key_t key;
    ppdb_value_t value;
    atomic_uint_least32_t marked;     // 标记节点是否已删除
    atomic_uint_least32_t level;      // 节点的层数
    atomic_uintptr_t next[];          // 每层的下一个节点指针
} ppdb_skiplist_node_t;

// 无锁跳表结构
struct ppdb_skiplist {
    atomic_uintptr_t head;            // 头节点
    atomic_size_t size;               // 元素数量
    uint32_t max_level;               // 最大层数
    ppdb_skiplist_config_t config;    // 配置
    ppdb_engine_sync_stats_t* stats;  // 统计信息
};

// 创建新节点
static ppdb_skiplist_node_t* create_node(const ppdb_key_t* key, const ppdb_value_t* value, uint32_t level) {
    size_t node_size = sizeof(ppdb_skiplist_node_t) + level * sizeof(atomic_uintptr_t);
    ppdb_skiplist_node_t* node = ppdb_engine_aligned_alloc(PPDB_ALIGNMENT, node_size);
    if (!node) return NULL;
    
    memcpy(&node->key, key, sizeof(ppdb_key_t));
    memcpy(&node->value, value, sizeof(ppdb_value_t));
    atomic_store(&node->marked, 0);
    atomic_store(&node->level, level);
    
    for (uint32_t i = 0; i < level; i++) {
        atomic_store(&node->next[i], (uintptr_t)NULL);
    }
    
    return node;
}

// 随机生成层数
static uint32_t random_level(ppdb_skiplist_t* list) {
    uint32_t level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF >> 1) && level < list->max_level) {
        level++;
    }
    return level;
}

// 查找节点和前驱节点
static bool find_node(ppdb_skiplist_t* list, const ppdb_key_t* key,
                     ppdb_skiplist_node_t** preds, ppdb_skiplist_node_t** succs) {
    ppdb_skiplist_node_t* pred = (ppdb_skiplist_node_t*)atomic_load(&list->head);
    ppdb_skiplist_node_t* curr = NULL;
    ppdb_skiplist_node_t* succ = NULL;
    
    for (int level = list->max_level - 1; level >= 0; level--) {
        curr = (ppdb_skiplist_node_t*)atomic_load(&pred->next[level]);
        
        while (curr) {
            succ = (ppdb_skiplist_node_t*)atomic_load(&curr->next[level]);
            
            // 如果当前节点被标记为删除
            while (atomic_load(&curr->marked)) {
                // 尝试物理删除节点
                if (!atomic_compare_exchange_weak(&pred->next[level],
                    (uintptr_t*)&curr, (uintptr_t)succ)) {
                    return false;  // CAS 失败，需要重试
                }
                curr = (ppdb_skiplist_node_t*)atomic_load(&pred->next[level]);
                if (!curr) break;
                succ = (ppdb_skiplist_node_t*)atomic_load(&curr->next[level]);
            }
            
            if (!curr || ppdb_key_compare(&curr->key, key) >= 0) {
                break;
            }
            pred = curr;
            curr = succ;
        }
        
        if (preds) preds[level] = pred;
        if (succs) succs[level] = curr;
    }
    
    return curr && ppdb_key_compare(&curr->key, key) == 0 && !atomic_load(&curr->marked);
}

// 插入节点
ppdb_error_t ppdb_skiplist_insert(ppdb_skiplist_t* list, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!list || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    uint32_t top_level = random_level(list);
    ppdb_skiplist_node_t* preds[PPDB_MAX_LEVEL];
    ppdb_skiplist_node_t* succs[PPDB_MAX_LEVEL];
    
    while (true) {
        bool found = find_node(list, key, preds, succs);
        
        // 如果找到了节点且未被标记删除，更新值
        if (found) {
            ppdb_skiplist_node_t* node = succs[0];
            if (!atomic_load(&node->marked)) {
                memcpy(&node->value, value, sizeof(ppdb_value_t));
                return PPDB_OK;
            }
            continue;  // 节点被标记删除，重试
        }
        
        // 创建新节点
        ppdb_skiplist_node_t* new_node = create_node(key, value, top_level);
        if (!new_node) return PPDB_ERR_OUT_OF_MEMORY;
        
        // 设置新节点的next指针
        for (uint32_t level = 0; level < top_level; level++) {
            atomic_store(&new_node->next[level], (uintptr_t)succs[level]);
        }
        
        // 尝试插入新节点
        ppdb_skiplist_node_t* pred = preds[0];
        ppdb_skiplist_node_t* succ = succs[0];
        
        if (!atomic_compare_exchange_strong(&pred->next[0],
            (uintptr_t*)&succ, (uintptr_t)new_node)) {
            ppdb_engine_free(new_node);
            continue;  // CAS失败，重试
        }
        
        // 插入其他层
        for (uint32_t level = 1; level < top_level; level++) {
            while (true) {
                pred = preds[level];
                succ = succs[level];
                
                if (atomic_compare_exchange_strong(&pred->next[level],
                    (uintptr_t*)&succ, (uintptr_t)new_node)) {
                    break;
                }
                
                find_node(list, key, preds, succs);  // 重新查找位置
            }
        }
        
        atomic_fetch_add(&list->size, 1);
        return PPDB_OK;
    }
}

// 删除节点
ppdb_error_t ppdb_skiplist_remove(ppdb_skiplist_t* list, const ppdb_key_t* key) {
    if (!list || !key) return PPDB_ERR_NULL_POINTER;
    
    ppdb_skiplist_node_t* preds[PPDB_MAX_LEVEL];
    ppdb_skiplist_node_t* succs[PPDB_MAX_LEVEL];
    ppdb_skiplist_node_t* node = NULL;
    
    while (true) {
        bool found = find_node(list, key, preds, succs);
        if (!found) {
            return PPDB_ERR_NOT_FOUND;
        }
        
        node = succs[0];
        
        // 标记节点为删除
        uint32_t unmarked = 0;
        if (!atomic_compare_exchange_strong(&node->marked,
            &unmarked, 1)) {
            continue;  // 节点已被标记删除，重试
        }
        
        // 物理删除节点
        bool physical_delete = true;
        for (uint32_t level = node->level - 1; level >= 0; level--) {
            ppdb_skiplist_node_t* succ = NULL;
            do {
                succ = (ppdb_skiplist_node_t*)atomic_load(&node->next[level]);
                physical_delete &= atomic_compare_exchange_strong(
                    &preds[level]->next[level],
                    (uintptr_t*)&node,
                    (uintptr_t)succ
                );
            } while (!physical_delete && atomic_load(&node->next[level]) == (uintptr_t)succ);
        }
        
        if (physical_delete) {
            atomic_fetch_sub(&list->size, 1);
        }
        
        return PPDB_OK;
    }
}

// 查找节点
ppdb_error_t ppdb_skiplist_find(ppdb_skiplist_t* list, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!list || !key || !value) return PPDB_ERR_NULL_POINTER;
    
    ppdb_skiplist_node_t* preds[PPDB_MAX_LEVEL];
    ppdb_skiplist_node_t* succs[PPDB_MAX_LEVEL];
    
    bool found = find_node(list, key, preds, succs);
    if (!found) {
        return PPDB_ERR_NOT_FOUND;
    }
    
    ppdb_skiplist_node_t* node = succs[0];
    memcpy(value, &node->value, sizeof(ppdb_value_t));
    
    return PPDB_OK;
}
