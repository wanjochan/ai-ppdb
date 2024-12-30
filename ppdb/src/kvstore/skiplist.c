#include <cosmopolitan.h>
#include <stdatomic.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/skiplist.h"
#include "internal/sync.h"
#include "internal/metrics.h"

// 节点状态
typedef enum node_state {
    NODE_VALID = 0,      // 正常节点
    NODE_DELETED = 1,    // 已标记删除
    NODE_INSERTING = 2   // 正在插入
} node_state_t;

// 节点结构 - 缓存行对齐优化
typedef struct skiplist_node {
    ppdb_sync_t sync;              // 同步原语
    void* key;                     // 键
    size_t key_len;               // 键长度
    void* value;                  // 值
    size_t value_len;             // 值长度
    uint32_t height;              // 高度
    _Atomic(struct skiplist_node*) next[] __attribute__((aligned(64)));  // 后继节点数组,缓存行对齐
} __attribute__((aligned(64))) skiplist_node_t;

// 跳表结构
struct ppdb_skiplist {
    ppdb_sync_t sync;             // 同步原语
    skiplist_node_t* head;        // 头节点
    uint32_t max_level;           // 最大层数
    atomic_size_t size;           // 节点数量
    atomic_size_t memory_usage;   // 内存使用
    bool enable_hint;             // 启用搜索提示
    struct {
        _Atomic(skiplist_node_t*) last_pos;  // 上次位置
        char prefix[8];                       // 前缀缓存
    } hint;
    ppdb_sync_config_t config;    // 同步配置
};

// 迭代器结构
typedef struct ppdb_skiplist_iterator {
    ppdb_skiplist_t* list;        // 跳表指针
    skiplist_node_t* current;     // 当前节点
    ppdb_sync_t sync;             // 迭代器同步
    atomic_uint32_t version;      // 迭代器版本
} ppdb_skiplist_iterator_t;

// 生成随机高度
static uint32_t random_height(uint32_t max_level) {
    uint32_t height = 1;
    while (height < max_level && ((double)rand() / RAND_MAX) < SKIPLIST_P) {
        height++;
    }
    return height;
}

// 创建节点 - 添加内存布局优化
static skiplist_node_t* create_node(const void* key, size_t key_len,
                                  const void* value, size_t value_len,
                                  uint32_t height,
                                  const ppdb_sync_config_t* config) {
    // 计算总大小,包括填充以对齐缓存行
    size_t node_size = sizeof(skiplist_node_t) + height * sizeof(skiplist_node_t*);
    node_size = (node_size + 63) & ~63;  // 向上取整到64字节

    // 分配对齐的内存
    skiplist_node_t* node = aligned_alloc(64, node_size);
    if (!node) return NULL;

    // 初始化同步原语
    ppdb_sync_init(&node->sync, config);
    
    // 复制数据
    node->key = malloc(key_len);
    node->value = malloc(value_len);
    if (!node->key || !node->value) {
        free(node->key);
        free(node->value);
        free(node);
        return NULL;
    }
    
    memcpy(node->key, key, key_len);
    memcpy(node->value, value, value_len);
    node->key_len = key_len;
    node->value_len = value_len;
    node->height = height;
    
    // 初始化next数组
    for (uint32_t i = 0; i < height; i++) {
        atomic_init(&node->next[i], NULL);
    }
    
    return node;
}

// 销毁节点 - 添加引用计数检查
static void destroy_node(skiplist_node_t* node) {
    if (!node) return;
    
    // 检查引用计数
    if (node->sync.ref_count && !ppdb_ref_dec(node->sync.ref_count)) {
        return;  // 还有其他引用
    }
    
    ppdb_sync_destroy(&node->sync);
    free(node->key);
    free(node->value);
    free(node);
}

// 比较键
static int compare_key(const void* key1, size_t key1_len,
                      const void* key2, size_t key2_len) {
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    return key1_len - key2_len;
}

// 创建跳表
ppdb_skiplist_t* ppdb_skiplist_create(const ppdb_skiplist_config_t* config) {
    if (!config) return NULL;

    ppdb_skiplist_t* list = aligned_alloc(64, sizeof(ppdb_skiplist_t));
    if (!list) return NULL;

    // 初始化同步原语
    ppdb_sync_init(&list->sync, &config->sync_config);
    list->config = config->sync_config;

    // 初始化基本字段
    list->max_level = config->max_level;
    atomic_init(&list->size, 0);
    atomic_init(&list->memory_usage, sizeof(ppdb_skiplist_t));
    list->enable_hint = config->enable_hint;
    atomic_init(&list->hint.last_pos, NULL);
    memset(list->hint.prefix, 0, sizeof(list->hint.prefix));

    // 创建头节点
    list->head = create_node(NULL, 0, NULL, 0, list->max_level, &list->config);
    if (!list->head) {
        ppdb_sync_destroy(&list->sync);
        free(list);
        return NULL;
    }

    return list;
}

// 销毁跳表
void ppdb_skiplist_destroy(ppdb_skiplist_t* list) {
    if (!list) return;

    ppdb_sync_lock(&list->sync);

    // 释放所有节点
    skiplist_node_t* current = list->head;
    while (current) {
        skiplist_node_t* next = current->next[0];
        destroy_node(current);
        current = next;
    }

    ppdb_sync_unlock(&list->sync);
    ppdb_sync_destroy(&list->sync);
    free(list);
}

// 插入数据 - 无锁实现
int ppdb_skiplist_insert(ppdb_skiplist_t* list, const void* key, size_t key_len,
                        const void* value, size_t value_len) {
    if (!list || !key || !value) return PPDB_ERROR;

    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current;
    int result = PPDB_ERROR;
    bool retry;

    do {
        retry = false;
        current = list->head;

        // 从顶层开始查找
        for (int i = list->max_level - 1; i >= 0; i--) {
            while (true) {
                skiplist_node_t* next = atomic_load(&current->next[i]);
                if (!next) break;

                // 检查节点状态
                if (!ppdb_sync_is_valid(&next->sync)) {
                    // 跳过已删除或正在插入的节点
                    current = next;
                    continue;
                }

                int cmp = compare_key(next->key, next->key_len, key, key_len);
                if (cmp >= 0) break;
                current = next;
            }
            update[i] = current;
        }

        // 检查是否已存在
        skiplist_node_t* next = atomic_load(&current->next[0]);
        if (next && ppdb_sync_is_valid(&next->sync) &&
            compare_key(next->key, next->key_len, key, key_len) == 0) {
            
            // 尝试更新值
            if (ppdb_sync_try_lock(&next->sync)) {
                void* new_value = malloc(value_len);
                if (!new_value) {
                    ppdb_sync_unlock(&next->sync);
                    return PPDB_ERROR;
                }

                void* old_value = next->value;
                size_t old_size = next->value_len;
                
                memcpy(new_value, value, value_len);
                next->value = new_value;
                next->value_len = value_len;
                
                atomic_fetch_add(&list->memory_usage, 
                               (ssize_t)value_len - (ssize_t)old_size);
                
                ppdb_sync_unlock(&next->sync);
                free(old_value);
                
                return PPDB_OK;
            }
            retry = true;
            continue;
        }

        // 创建新节点
        uint32_t height = random_height(list->max_level);
        skiplist_node_t* node = create_node(key, key_len, value, value_len, 
                                          height, &list->config);
        if (!node) return PPDB_ERROR;

        // 标记节点为插入状态
        if (!ppdb_sync_mark_inserting(&node->sync)) {
            destroy_node(node);
            return PPDB_ERROR;
        }

        // 原子插入
        bool success = true;
        for (uint32_t i = 0; i < height && success; i++) {
            do {
                skiplist_node_t* expected = update[i]->next[i];
                atomic_store(&node->next[i], expected);
                success = atomic_compare_exchange_strong(&update[i]->next[i],
                                                       &expected, node);
            } while (!success && 
                    (!expected || ppdb_sync_is_valid(&expected->sync)));
        }

        if (!success) {
            // 插入失败,清理并重试
            destroy_node(node);
            retry = true;
            continue;
        }

        // 更新统计信息
        atomic_fetch_add(&list->size, 1);
        atomic_fetch_add(&list->memory_usage, 
                        sizeof(skiplist_node_t) + height * sizeof(skiplist_node_t*) +
                        key_len + value_len);

        // 更新搜索提示
        if (list->enable_hint) {
            atomic_store(&list->hint.last_pos, node);
            if (key_len >= sizeof(list->hint.prefix)) {
                memcpy(list->hint.prefix, key, sizeof(list->hint.prefix));
            }
        }

        // 标记节点为有效
        ppdb_sync_init(&node->sync, &list->config);
        result = PPDB_OK;

    } while (retry);

    return result;
}

// 查找数据 - 无锁实现
int ppdb_skiplist_find(ppdb_skiplist_t* list, const void* key, size_t key_len,
                      void** value, size_t* value_len) {
    if (!list || !key || !value || !value_len) return PPDB_ERROR;

    skiplist_node_t* current = list->head;
    bool retry;

    do {
        retry = false;

        // 使用搜索提示
        if (list->enable_hint) {
            skiplist_node_t* hint = atomic_load(&list->hint.last_pos);
            if (hint && key_len >= sizeof(list->hint.prefix) &&
                memcmp(key, list->hint.prefix, sizeof(list->hint.prefix)) == 0) {
                
                // 验证提示节点状态
                if (ppdb_sync_is_valid(&hint->sync)) {
                    current = hint;
                }
            }
        }

        // 从顶层到底层搜索
        for (int i = list->max_level - 1; i >= 0; i--) {
            while (true) {
                skiplist_node_t* next = atomic_load(&current->next[i]);
                if (!next) break;

                // 检查节点状态
                if (!ppdb_sync_is_valid(&next->sync)) {
                    current = next;
                    continue;
                }

                int cmp = compare_key(next->key, next->key_len, key, key_len);
                if (cmp >= 0) break;
                current = next;
            }
        }

        // 获取目标节点
        skiplist_node_t* target = atomic_load(&current->next[0]);
        if (!target || !ppdb_sync_is_valid(&target->sync) ||
            compare_key(target->key, target->key_len, key, key_len) != 0) {
            return PPDB_NOT_FOUND;
        }

        // 尝试锁定节点读取值
        if (ppdb_sync_try_lock(&target->sync)) {
            *value = malloc(target->value_len);
            if (!*value) {
                ppdb_sync_unlock(&target->sync);
                return PPDB_ERROR;
            }

            memcpy(*value, target->value, target->value_len);
            *value_len = target->value_len;

            // 更新搜索提示
            if (list->enable_hint) {
                atomic_store(&list->hint.last_pos, target);
                if (key_len >= sizeof(list->hint.prefix)) {
                    memcpy(list->hint.prefix, key, sizeof(list->hint.prefix));
                }
            }

            ppdb_sync_unlock(&target->sync);
            return PPDB_OK;
        }

        retry = true;
    } while (retry);

    return PPDB_ERROR;
}

// 删除数据 - 无锁实现
int ppdb_skiplist_remove(ppdb_skiplist_t* list, const void* key, size_t key_len) {
    if (!list || !key) return PPDB_ERROR;

    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current;
    bool retry;
    int result = PPDB_ERROR;

    do {
        retry = false;
        current = list->head;

        // 从顶层开始查找
        for (int i = list->max_level - 1; i >= 0; i--) {
            while (true) {
                skiplist_node_t* next = atomic_load(&current->next[i]);
                if (!next) break;

                // 检查节点状态
                if (!ppdb_sync_is_valid(&next->sync)) {
                    current = next;
                    continue;
                }

                int cmp = compare_key(next->key, next->key_len, key, key_len);
                if (cmp >= 0) break;
                current = next;
            }
            update[i] = current;
        }

        // 获取目标节点
        skiplist_node_t* target = atomic_load(&current->next[0]);
        if (!target || !ppdb_sync_is_valid(&target->sync) ||
            compare_key(target->key, target->key_len, key, key_len) != 0) {
            return PPDB_NOT_FOUND;
        }

        // 尝试标记节点为删除状态
        if (!ppdb_sync_mark_deleted(&target->sync)) {
            retry = true;
            continue;
        }

        // 原子更新所有层的指针
        bool success = true;
        for (uint32_t i = 0; i < target->height && success; i++) {
            do {
                skiplist_node_t* expected = target;
                success = atomic_compare_exchange_strong(&update[i]->next[i],
                                                       &expected,
                                                       atomic_load(&target->next[i]));
            } while (!success && ppdb_sync_is_valid(&expected->sync));
        }

        if (!success) {
            retry = true;
            continue;
        }

        // 更新统计信息
        atomic_fetch_sub(&list->size, 1);
        atomic_fetch_sub(&list->memory_usage,
                        sizeof(skiplist_node_t) + target->height * sizeof(skiplist_node_t*) +
                        target->key_len + target->value_len);

        // 更新搜索提示
        if (list->enable_hint && atomic_load(&list->hint.last_pos) == target) {
            atomic_store(&list->hint.last_pos, NULL);
            memset(list->hint.prefix, 0, sizeof(list->hint.prefix));
        }

        // 延迟删除节点
        destroy_node(target);
        result = PPDB_OK;

    } while (retry);

    return result;
}

// 创建迭代器
ppdb_skiplist_iterator_t* ppdb_skiplist_iterator_create(ppdb_skiplist_t* list) {
    if (!list) return NULL;

    ppdb_skiplist_iterator_t* iter = aligned_alloc(64, sizeof(ppdb_skiplist_iterator_t));
    if (!iter) return NULL;

    // 初始化迭代器
    iter->list = list;
    iter->current = list->head;
    atomic_init(&iter->version, 0);
    ppdb_sync_init(&iter->sync, &list->config);

    return iter;
}

// 销毁迭代器
void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* iter) {
    if (!iter) return;

    ppdb_sync_destroy(&iter->sync);
    free(iter);
}

// 迭代器移动到下一个有效节点
static skiplist_node_t* iterator_next_valid(ppdb_skiplist_iterator_t* iter) {
    skiplist_node_t* current = iter->current;
    
    while (true) {
        skiplist_node_t* next = atomic_load(&current->next[0]);
        if (!next) return NULL;

        // 检查节点状态
        if (ppdb_sync_is_valid(&next->sync)) {
            return next;
        }
        current = next;
    }
}

// 迭代器移动到下一个节点
int ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* iter) {
    if (!iter) return PPDB_ERROR;

    ppdb_sync_lock(&iter->sync);

    // 获取下一个有效节点
    skiplist_node_t* next = iterator_next_valid(iter);
    if (!next) {
        ppdb_sync_unlock(&iter->sync);
        return PPDB_NOT_FOUND;
    }

    iter->current = next;
    atomic_fetch_add(&iter->version, 1);

    ppdb_sync_unlock(&iter->sync);
    return PPDB_OK;
}

// 获取当前节点的键值
int ppdb_skiplist_iterator_get(ppdb_skiplist_iterator_t* iter,
                             void** key, size_t* key_len,
                             void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return PPDB_ERROR;
    }

    ppdb_sync_lock(&iter->sync);

    skiplist_node_t* current = iter->current;
    if (current == iter->list->head) {
        ppdb_sync_unlock(&iter->sync);
        return PPDB_NOT_FOUND;
    }

    // 检查节点状态
    if (!ppdb_sync_is_valid(&current->sync)) {
        ppdb_sync_unlock(&iter->sync);
        return PPDB_ERROR;
    }

    // 分配并复制键值
    *key = malloc(current->key_len);
    *value = malloc(current->value_len);
    if (!*key || !*value) {
        free(*key);
        free(*value);
        ppdb_sync_unlock(&iter->sync);
        return PPDB_ERROR;
    }

    memcpy(*key, current->key, current->key_len);
    memcpy(*value, current->value, current->value_len);
    *key_len = current->key_len;
    *value_len = current->value_len;

    ppdb_sync_unlock(&iter->sync);
    return PPDB_OK;
}

// 重置迭代器到起始位置
int ppdb_skiplist_iterator_reset(ppdb_skiplist_iterator_t* iter) {
    if (!iter) return PPDB_ERROR;

    ppdb_sync_lock(&iter->sync);
    iter->current = iter->list->head;
    atomic_store(&iter->version, 0);
    ppdb_sync_unlock(&iter->sync);

    return PPDB_OK;
}

// 检查迭代器是否有效
bool ppdb_skiplist_iterator_valid(ppdb_skiplist_iterator_t* iter) {
    if (!iter) return false;

    ppdb_sync_lock(&iter->sync);
    bool valid = (iter->current != NULL && 
                 iter->current != iter->list->head &&
                 ppdb_sync_is_valid(&iter->current->sync));
    ppdb_sync_unlock(&iter->sync);

    return valid;
}
