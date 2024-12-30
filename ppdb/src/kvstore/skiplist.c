#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/skiplist.h"

// 跳表最大层数
#define PPDB_SKIPLIST_MAX_LEVEL 32

// 默认比较函数
int ppdb_skiplist_default_compare(const void* key1, size_t key1_len,
                                const void* key2, size_t key2_len) {
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    return (key1_len < key2_len) ? -1 : (key1_len > key2_len) ? 1 : 0;
}

// 内部函数声明
static ppdb_skiplist_node_t* create_node(const void* key, size_t key_len,
                                       const void* value, size_t value_len,
                                       int level);
static void destroy_node(ppdb_skiplist_node_t* node);
static int random_level(int max_level);

// 创建跳表
ppdb_error_t ppdb_skiplist_create(ppdb_skiplist_t** list,
                                int max_level,
                                ppdb_compare_func_t compare_func,
                                const ppdb_sync_config_t* sync_config) {
    if (!list || !compare_func || !sync_config) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 分配跳表结构
    ppdb_skiplist_t* new_list = calloc(1, sizeof(ppdb_skiplist_t));
    if (!new_list) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化跳表
    new_list->max_level = max_level > 0 ? max_level : PPDB_SKIPLIST_MAX_LEVEL;
    new_list->size = 0;
    new_list->memory_usage = sizeof(ppdb_skiplist_t);
    new_list->compare = compare_func;

    // 初始化同步原语
    ppdb_error_t err = ppdb_sync_init(&new_list->sync, sync_config);
    if (err != PPDB_OK) {
        free(new_list);
        return err;
    }

    // 创建头节点（不包含实际数据）
    new_list->head = create_node(NULL, 0, NULL, 0, new_list->max_level);
    if (!new_list->head) {
        ppdb_sync_destroy(&new_list->sync);
        free(new_list);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化头节点的next数组
    for (int i = 0; i < new_list->max_level; i++) {
        new_list->head->next[i] = NULL;
    }

    *list = new_list;
    return PPDB_OK;
}

// 销毁跳表
void ppdb_skiplist_destroy(ppdb_skiplist_t* list) {
    if (!list) return;
    
    // 销毁所有节点
    if (list->head) {
        ppdb_skiplist_node_t* current = list->head;
        ppdb_skiplist_node_t* next = NULL;
        
        while (current) {
            next = current->next[0];
            destroy_node(current);
            current = next;
        }
        list->head = NULL;
    }
    
    // 销毁同步原语
    ppdb_sync_destroy(&list->sync);
    
    // 释放跳表结构
    free(list);
}

// 创建节点
static ppdb_skiplist_node_t* create_node(const void* key, size_t key_len,
                                      const void* value, size_t value_len,
                                      int level) {
    if (level <= 0) return NULL;

    // 计算节点大小（包括next数组）
    size_t node_size = sizeof(ppdb_skiplist_node_t) + level * sizeof(ppdb_skiplist_node_t*);
    
    // 分配节点内存（包括next数组）
    ppdb_skiplist_node_t* node = calloc(1, node_size);
    if (!node) return NULL;

    // 初始化next数组指针
    node->next = (ppdb_skiplist_node_t**)((char*)node + sizeof(ppdb_skiplist_node_t));
    memset(node->next, 0, level * sizeof(ppdb_skiplist_node_t*));

    // 分配键值内存
    if (key && key_len > 0) {
        node->key = malloc(key_len);
        if (!node->key) {
            free(node);
            return NULL;
        }
        memcpy(node->key, key, key_len);
        node->key_len = key_len;
    } else {
        node->key = NULL;
        node->key_len = 0;
    }

    if (value && value_len > 0) {
        node->value = malloc(value_len);
        if (!node->value) {
            if (node->key) free(node->key);
            free(node);
            return NULL;
        }
        memcpy(node->value, value, value_len);
        node->value_len = value_len;
    } else {
        node->value = NULL;
        node->value_len = 0;
    }

    node->level = level;
    return node;
}

// 销毁节点
static void destroy_node(ppdb_skiplist_node_t* node) {
    if (!node) return;
    
    // 释放键
    if (node->key) {
        free(node->key);
        node->key = NULL;
    }
    
    // 释放值
    if (node->value) {
        free(node->value);
        node->value = NULL;
    }
    
    // 释放节点（next数组是和node一起分配的）
    free(node);
}

// 生成随机层数
static int random_level(int max_level) {
    int level = 1;
    while (level < max_level && (rand() % 2) == 0) {
        level++;
    }
    return level;
}

// 插入键值对
ppdb_error_t ppdb_skiplist_put(ppdb_skiplist_t* list,
                             const void* key, size_t key_len,
                             const void* value, size_t value_len) {
    if (!list || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_skiplist_node_t* update[PPDB_SKIPLIST_MAX_LEVEL];
    ppdb_skiplist_node_t* current = list->head;

    // 查找插入位置
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && 
               list->compare(current->next[i]->key, current->next[i]->key_len,
                           key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    current = current->next[0];

    // 如果键已存在，更新值
    if (current && list->compare(current->key, current->key_len,
                                key, key_len) == 0) {
        void* new_value = aligned_alloc(64, value_len);
        if (!new_value) {
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        memcpy(new_value, value, value_len);
        if (current->value) {
            free(current->value);
        }
        current->value = new_value;
        current->value_len = value_len;
        return PPDB_OK;
    }

    // 创建新节点
    int level = random_level(list->max_level);
    ppdb_skiplist_node_t* new_node = create_node(key, key_len, value, value_len, level);
    if (!new_node) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 插入新节点
    for (int i = 0; i < level; i++) {
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
    }

    list->size++;
    list->memory_usage += sizeof(ppdb_skiplist_node_t) + level * sizeof(ppdb_skiplist_node_t*) +
                         key_len + value_len;

    return PPDB_OK;
}

// 获取键值对
ppdb_error_t ppdb_skiplist_get(ppdb_skiplist_t* list,
                             const void* key, size_t key_len,
                             void** value, size_t* value_len) {
    if (!list || !key || !value || !value_len) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_skiplist_node_t* current = list->head;

    // 查找节点
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && 
               list->compare(current->next[i]->key, current->next[i]->key_len,
                           key, key_len) < 0) {
            current = current->next[i];
        }
    }

    current = current->next[0];

    // 检查是否找到
    if (current && list->compare(current->key, current->key_len,
                                key, key_len) == 0) {
        *value = aligned_alloc(64, current->value_len);
        if (!*value) {
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memcpy(*value, current->value, current->value_len);
        *value_len = current->value_len;
        return PPDB_OK;
    }

    return PPDB_ERR_NOT_FOUND;
}

// 删除键值对
ppdb_error_t ppdb_skiplist_delete(ppdb_skiplist_t* list,
                                const void* key, size_t key_len) {
    if (!list || !key) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_skiplist_node_t* update[PPDB_SKIPLIST_MAX_LEVEL];
    ppdb_skiplist_node_t* current = list->head;

    // 查找删除位置
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && 
               list->compare(current->next[i]->key, current->next[i]->key_len,
                           key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    current = current->next[0];

    // 检查是否找到
    if (!current || list->compare(current->key, current->key_len,
                                 key, key_len) != 0) {
        return PPDB_ERR_NOT_FOUND;
    }

    // 更新指针
    for (int i = 0; i < current->level; i++) {
        update[i]->next[i] = current->next[i];
    }

    list->size--;
    list->memory_usage -= sizeof(ppdb_skiplist_node_t) + 
                         current->level * sizeof(ppdb_skiplist_node_t*) +
                         current->key_len + current->value_len;

    destroy_node(current);
    return PPDB_OK;
}

// 获取大小
size_t ppdb_skiplist_size(ppdb_skiplist_t* list) {
    return list ? list->size : 0;
}

// 获取内存使用量
size_t ppdb_skiplist_memory_usage(ppdb_skiplist_t* list) {
    return list ? list->memory_usage : 0;
}

// 检查是否为空
bool ppdb_skiplist_empty(ppdb_skiplist_t* list) {
    return list ? list->size == 0 : true;
}

// 清空跳表
void ppdb_skiplist_clear(ppdb_skiplist_t* list) {
    if (!list) return;

    ppdb_skiplist_node_t* current = list->head->next[0];
    while (current) {
        ppdb_skiplist_node_t* next = current->next[0];
        destroy_node(current);
        current = next;
    }

    for (int i = 0; i < list->max_level; i++) {
        list->head->next[i] = NULL;
    }

    list->size = 0;
    list->memory_usage = sizeof(ppdb_skiplist_t) + 
                        sizeof(ppdb_skiplist_node_t) + 
                        list->max_level * sizeof(ppdb_skiplist_node_t*);
}

// 创建迭代器
ppdb_error_t ppdb_skiplist_iterator_create(ppdb_skiplist_t* list,
                                        ppdb_skiplist_iterator_t** iter,
                                        const ppdb_sync_config_t* sync_config) {
    if (!list || !iter || !sync_config) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_skiplist_iterator_t* new_iter = aligned_alloc(64, sizeof(ppdb_skiplist_iterator_t));
    if (!new_iter) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化迭代器
    new_iter->list = list;
    new_iter->current = list->head;
    new_iter->valid = true;
    memset(&new_iter->current_pair, 0, sizeof(ppdb_kv_pair_t));

    // 初始化同步原语
    ppdb_error_t err = ppdb_sync_init(&new_iter->sync, sync_config);
    if (err != PPDB_OK) {
        free(new_iter);
        return err;
    }

    *iter = new_iter;
    return PPDB_OK;
}

// 销毁迭代器
void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* iter) {
    if (!iter) return;
    ppdb_sync_destroy(&iter->sync);
    free(iter);
}

// 迭代器移动到下一个元素
ppdb_error_t ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* iter,
                                      void** key, size_t* key_len,
                                      void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (!iter->valid) {
        return PPDB_ERR_ITERATOR_END;
    }

    // 移动到下一个节点
    iter->current = iter->current->next[0];
    if (!iter->current) {
        iter->valid = false;
        return PPDB_ERR_ITERATOR_END;
    }

    // 复制键值对
    *key = aligned_alloc(64, iter->current->key_len);
    if (!*key) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    *value = aligned_alloc(64, iter->current->value_len);
    if (!*value) {
        free(*key);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    memcpy(*key, iter->current->key, iter->current->key_len);
    memcpy(*value, iter->current->value, iter->current->value_len);
    *key_len = iter->current->key_len;
    *value_len = iter->current->value_len;

    return PPDB_OK;
}

// 检查迭代器是否有效
bool ppdb_skiplist_iterator_valid(ppdb_skiplist_iterator_t* iter) {
    return iter && iter->valid;
}

// 获取迭代器当前键值对
ppdb_error_t ppdb_skiplist_iterator_get(ppdb_skiplist_iterator_t* iter,
                                     ppdb_kv_pair_t* pair) {
    if (!iter || !pair) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (!iter->valid || !iter->current) {
        return PPDB_ERR_ITERATOR_END;
    }

    // 复制键
    pair->key = aligned_alloc(64, iter->current->key_len);
    if (!pair->key) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(pair->key, iter->current->key, iter->current->key_len);
    pair->key_size = iter->current->key_len;

    // 复制值
    pair->value = aligned_alloc(64, iter->current->value_len);
    if (!pair->value) {
        free(pair->key);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(pair->value, iter->current->value, iter->current->value_len);
    pair->value_size = iter->current->value_len;

    return PPDB_OK;
}

// 无锁写入键值对
ppdb_error_t ppdb_skiplist_put_lockfree(ppdb_skiplist_t* list,
                                      const void* key, size_t key_len,
                                      const void* value, size_t value_len) {
    if (!list || !key || !value || key_len == 0 || value_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 创建新节点
    int level = random_level(list->max_level);
    ppdb_skiplist_node_t* new_node = create_node(key, key_len, value, value_len, level);
    if (!new_node) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 更新节点指针
    ppdb_skiplist_node_t* current = list->head;
    ppdb_skiplist_node_t* update[PPDB_SKIPLIST_MAX_LEVEL];
    memset(update, 0, sizeof(update));

    // 从最高层开始查找
    for (int i = level - 1; i >= 0; i--) {
        while (current->next[i] && 
               list->compare(current->next[i]->key, current->next[i]->key_len,
                           key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    // 检查是否已存在相同的键
    current = current->next[0];
    if (current && list->compare(current->key, current->key_len,
                               key, key_len) == 0) {
        // 更新现有节点的值
        void* new_value = aligned_alloc(64, value_len);
        if (!new_value) {
            destroy_node(new_node);
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        memcpy(new_value, value, value_len);
        if (current->value) {
            free(current->value);
        }
        current->value = new_value;
        current->value_len = value_len;

        destroy_node(new_node);
        return PPDB_OK;
    }

    // 插入新节点
    for (int i = 0; i < level; i++) {
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
    }

    atomic_fetch_add(&list->size, 1);
    atomic_fetch_add(&list->memory_usage, sizeof(ppdb_skiplist_node_t) + 
                                        level * sizeof(ppdb_skiplist_node_t*) +
                                        key_len + value_len);
    return PPDB_OK;
}

// 无锁读取键值对
ppdb_error_t ppdb_skiplist_get_lockfree(ppdb_skiplist_t* list,
                                      const void* key, size_t key_len,
                                      void** value, size_t* value_len) {
    if (!list || !key || !value || !value_len || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 从最高层开始查找
    ppdb_skiplist_node_t* current = list->head;
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && 
               list->compare(current->next[i]->key, current->next[i]->key_len,
                           key, key_len) < 0) {
            current = current->next[i];
        }
    }

    // 移动到下一个节点
    current = current->next[0];

    // 检查是否找到
    if (current && list->compare(current->key, current->key_len,
                               key, key_len) == 0) {
        *value = aligned_alloc(64, current->value_len);
        if (!*value) {
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        memcpy(*value, current->value, current->value_len);
        *value_len = current->value_len;
        return PPDB_OK;
    }

    return PPDB_ERR_NOT_FOUND;
}

// 无锁删除键值对
ppdb_error_t ppdb_skiplist_delete_lockfree(ppdb_skiplist_t* list,
                                         const void* key, size_t key_len) {
    if (!list || !key || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 更新节点指针
    ppdb_skiplist_node_t* current = list->head;
    ppdb_skiplist_node_t* update[PPDB_SKIPLIST_MAX_LEVEL];
    memset(update, 0, sizeof(update));

    // 从最高层开始查找
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && 
               list->compare(current->next[i]->key, current->next[i]->key_len,
                           key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    // 移动到目标节点
    current = current->next[0];

    // 检查是否找到
    if (!current || list->compare(current->key, current->key_len,
                                key, key_len) != 0) {
        return PPDB_ERR_NOT_FOUND;
    }

    // 更新指针
    for (int i = 0; i < list->max_level; i++) {
        if (update[i]->next[i] != current) {
            break;
        }
        update[i]->next[i] = current->next[i];
    }

    size_t node_size = sizeof(ppdb_skiplist_node_t) + 
                      current->level * sizeof(ppdb_skiplist_node_t*) +
                      current->key_len + current->value_len;

    atomic_fetch_sub(&list->size, 1);
    atomic_fetch_sub(&list->memory_usage, node_size);

    destroy_node(current);
    return PPDB_OK;
}
