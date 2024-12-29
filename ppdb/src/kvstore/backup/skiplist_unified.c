#include "skiplist_unified.h"
#include <stdlib.h>
#include <string.h>
#include "ppdb/hash.h"

// 内部工具函数
static inline void* NODE_KEY(ppdb_skiplist_node_t* node) {
    return (void*)(node->next + node->level);
}

static inline void* NODE_VALUE(ppdb_skiplist_node_t* node) {
    return (void*)((char*)NODE_KEY(node) + node->key_len);
}

static ppdb_skiplist_node_t* skiplist_alloc_node(uint32_t level,
                                                size_t key_len,
                                                size_t value_len) {
    size_t node_size = sizeof(ppdb_skiplist_node_t) +
                      level * sizeof(ppdb_skiplist_node_t*) +
                      key_len + value_len;
    
    ppdb_skiplist_node_t* node = aligned_alloc(64, node_size);
    if (node) {
        node->level = level;
        node->key_len = key_len;
        node->value_len = value_len;
        memset(node->next, 0, level * sizeof(ppdb_skiplist_node_t*));
    }
    return node;
}

static uint32_t random_level(uint32_t max_level) {
    uint32_t level = 1;
    while (level < max_level && (rand() & 0xFFFF) < 0x3FFF) {
        level++;
    }
    return level;
}

// 创建跳表
ppdb_skiplist_t* ppdb_skiplist_create(const ppdb_skiplist_config_t* config) {
    ppdb_skiplist_t* list = calloc(1, sizeof(ppdb_skiplist_t));
    if (!list) return NULL;
    
    list->max_level = config->max_level;
    memcpy(&list->config, config, sizeof(ppdb_skiplist_config_t));
    
    // 创建头节点
    list->head = skiplist_alloc_node(config->max_level, 0, 0);
    if (!list->head) {
        free(list);
        return NULL;
    }
    
    // 初始化同步机制
    if (config->sync_config.stripe_count > 0) {
        list->sync.stripes = ppdb_stripe_locks_create(&config->sync_config);
        if (!list->sync.stripes) {
            free(list->head);
            free(list);
            return NULL;
        }
    } else {
        ppdb_sync_init(&list->sync.global_lock, &config->sync_config);
    }
    
    // 初始化查找提示缓存
    if (config->enable_hint) {
        list->opt.hints = calloc(256, sizeof(struct skip_hint_t));
    }
    
    return list;
}

void ppdb_skiplist_destroy(ppdb_skiplist_t* list) {
    if (!list) return;
    
    // 清理所有节点
    ppdb_skiplist_node_t* node = list->head;
    while (node) {
        ppdb_skiplist_node_t* next = node->next[0];
        free(node);
        node = next;
    }
    
    // 清理同步机制
    if (list->config.sync_config.stripe_count > 0) {
        ppdb_stripe_locks_destroy(list->sync.stripes);
    } else {
        ppdb_sync_destroy(&list->sync.global_lock);
    }
    
    // 清理查找提示缓存
    if (list->opt.hints) {
        free(list->opt.hints);
    }
    
    free(list);
}

// 查找函数
static ppdb_skiplist_node_t* skiplist_find_node(ppdb_skiplist_t* list,
                                               const void* key,
                                               size_t key_len,
                                               ppdb_skiplist_node_t** update) {
    ppdb_skiplist_node_t* node = list->head;
    
    // 使用查找提示
    if (list->config.enable_hint && list->opt.hints) {
        struct skip_hint_t* hint = &list->opt.hints[ppdb_hash(key, key_len) & 0xFF];
        if (hint->last_pos && memcmp(NODE_KEY(hint->last_pos), key, 
            min(key_len, hint->last_pos->key_len)) <= 0) {
            node = hint->last_pos;
        }
    }
    
    // 从高层向低层查找
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (node->next[i] && memcmp(NODE_KEY(node->next[i]), key, key_len) < 0) {
            node = node->next[i];
        }
        if (update) update[i] = node;
    }
    
    return node->next[0];
}

// 插入操作
int ppdb_skiplist_insert(ppdb_skiplist_t* list,
                        const void* key, size_t key_len,
                        const void* value, size_t value_len) {
    // 检查内存限制
    if (atomic_load(&list->stats.mem_used) + key_len + value_len > 
        list->config.max_size) {
        return PPDB_ERR_NO_MEMORY;
    }
    
    // 获取锁
    bool locked;
    if (list->config.sync_config.stripe_count > 0) {
        locked = ppdb_stripe_locks_try_lock(list->sync.stripes, key, key_len);
    } else {
        locked = ppdb_sync_try_lock(&list->sync.global_lock);
    }
    
    if (!locked) {
        atomic_fetch_add(&list->stats.conflicts, 1);
        return PPDB_ERR_BUSY;
    }
    
    // 查找插入位置
    ppdb_skiplist_node_t* update[PPDB_MAX_LEVEL];
    ppdb_skiplist_node_t* node = skiplist_find_node(list, key, key_len, update);
    
    // 如果key已存在，更新value
    if (node && node->key_len == key_len && 
        memcmp(NODE_KEY(node), key, key_len) == 0) {
        memcpy(NODE_VALUE(node), value, value_len);
        goto done;
    }
    
    // 创建新节点
    uint32_t level = random_level(list->max_level);
    ppdb_skiplist_node_t* new_node = skiplist_alloc_node(level, key_len, value_len);
    if (!new_node) {
        goto error;
    }
    
    // 复制key和value
    memcpy(NODE_KEY(new_node), key, key_len);
    memcpy(NODE_VALUE(new_node), value, value_len);
    
    // 更新指针
    for (uint32_t i = 0; i < level; i++) {
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
    }
    
    // 更新统计信息
    atomic_fetch_add(&list->stats.mem_used, key_len + value_len);
    atomic_fetch_add(&list->stats.ops_count, 1);
    list->size++;
    
done:
    // 释放锁
    if (list->config.sync_config.stripe_count > 0) {
        ppdb_stripe_locks_unlock(list->sync.stripes, key, key_len);
    } else {
        ppdb_sync_unlock(&list->sync.global_lock);
    }
    return PPDB_OK;
    
error:
    if (list->config.sync_config.stripe_count > 0) {
        ppdb_stripe_locks_unlock(list->sync.stripes, key, key_len);
    } else {
        ppdb_sync_unlock(&list->sync.global_lock);
    }
    return PPDB_ERR_NO_MEMORY;
}

// 查找操作
int ppdb_skiplist_find(ppdb_skiplist_t* list,
                      const void* key, size_t key_len,
                      void** value, size_t* value_len) {
    // 获取锁（读操作）
    bool locked;
    if (list->config.sync_config.stripe_count > 0) {
        locked = ppdb_stripe_locks_try_lock(list->sync.stripes, key, key_len);
    } else {
        locked = ppdb_sync_try_lock(&list->sync.global_lock);
    }
    
    if (!locked) {
        atomic_fetch_add(&list->stats.conflicts, 1);
        return PPDB_ERR_BUSY;
    }
    
    // 查找节点
    ppdb_skiplist_node_t* node = skiplist_find_node(list, key, key_len, NULL);
    
    int ret = PPDB_OK;
    if (node && node->key_len == key_len && 
        memcmp(NODE_KEY(node), key, key_len) == 0) {
        *value = NODE_VALUE(node);
        *value_len = node->value_len;
        
        // 更新查找提示
        if (list->config.enable_hint && list->opt.hints) {
            struct skip_hint_t* hint = &list->opt.hints[ppdb_hash(key, key_len) & 0xFF];
            hint->last_pos = node;
        }
    } else {
        ret = PPDB_ERR_NOT_FOUND;
    }
    
    // 释放锁
    if (list->config.sync_config.stripe_count > 0) {
        ppdb_stripe_locks_unlock(list->sync.stripes, key, key_len);
    } else {
        ppdb_sync_unlock(&list->sync.global_lock);
    }
    
    return ret;
}

// 删除操作
int ppdb_skiplist_remove(ppdb_skiplist_t* list,
                        const void* key, size_t key_len) {
    // 获取锁
    bool locked;
    if (list->config.sync_config.stripe_count > 0) {
        locked = ppdb_stripe_locks_try_lock(list->sync.stripes, key, key_len);
    } else {
        locked = ppdb_sync_try_lock(&list->sync.global_lock);
    }
    
    if (!locked) {
        atomic_fetch_add(&list->stats.conflicts, 1);
        return PPDB_ERR_BUSY;
    }
    
    // 查找要删除的节点
    ppdb_skiplist_node_t* update[PPDB_MAX_LEVEL];
    ppdb_skiplist_node_t* node = skiplist_find_node(list, key, key_len, update);
    
    if (!node || node->key_len != key_len || 
        memcmp(NODE_KEY(node), key, key_len) != 0) {
        goto not_found;
    }
    
    // 更新指针
    for (uint32_t i = 0; i < node->level; i++) {
        update[i]->next[i] = node->next[i];
    }
    
    // 更新统计信息
    atomic_fetch_sub(&list->stats.mem_used, node->key_len + node->value_len);
    atomic_fetch_add(&list->stats.ops_count, 1);
    list->size--;
    
    // 清理查找提示
    if (list->config.enable_hint && list->opt.hints) {
        struct skip_hint_t* hint = &list->opt.hints[ppdb_hash(key, key_len) & 0xFF];
        if (hint->last_pos == node) {
            hint->last_pos = NULL;
        }
    }
    
    free(node);
    
    // 释放锁
    if (list->config.sync_config.stripe_count > 0) {
        ppdb_stripe_locks_unlock(list->sync.stripes, key, key_len);
    } else {
        ppdb_sync_unlock(&list->sync.global_lock);
    }
    return PPDB_OK;
    
not_found:
    if (list->config.sync_config.stripe_count > 0) {
        ppdb_stripe_locks_unlock(list->sync.stripes, key, key_len);
    } else {
        ppdb_sync_unlock(&list->sync.global_lock);
    }
    return PPDB_ERR_NOT_FOUND;
}

// 迭代器实现
ppdb_skiplist_iter_t* ppdb_skiplist_iter_create(ppdb_skiplist_t* list) {
    ppdb_skiplist_iter_t* iter = malloc(sizeof(ppdb_skiplist_iter_t));
    if (!iter) return NULL;
    
    iter->list = list;
    iter->current = list->head->next[0];
    iter->key_buf = NULL;
    iter->key_size = 0;
    
    return iter;
}

void ppdb_skiplist_iter_destroy(ppdb_skiplist_iter_t* iter) {
    if (!iter) return;
    if (iter->key_buf) free(iter->key_buf);
    free(iter);
}

bool ppdb_skiplist_iter_valid(ppdb_skiplist_iter_t* iter) {
    return iter && iter->current != NULL;
}

void ppdb_skiplist_iter_next(ppdb_skiplist_iter_t* iter) {
    if (!iter || !iter->current) return;
    iter->current = iter->current->next[0];
}

const void* ppdb_skiplist_iter_key(ppdb_skiplist_iter_t* iter, size_t* len) {
    if (!iter || !iter->current) return NULL;
    *len = iter->current->key_len;
    return NODE_KEY(iter->current);
}

const void* ppdb_skiplist_iter_value(ppdb_skiplist_iter_t* iter, size_t* len) {
    if (!iter || !iter->current) return NULL;
    *len = iter->current->value_len;
    return NODE_VALUE(iter->current);
}
