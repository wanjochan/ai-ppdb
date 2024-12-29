#include "ppdb/skiplist/skiplist.h"
#include "ppdb/common/sync.h"
#include <stdlib.h>
#include <string.h>

#define MAX_LEVEL 32
#define SKIPLIST_P 0.25

// 节点结构
typedef struct skiplist_node {
    void* key;                     // 键
    size_t key_len;               // 键长度
    void* value;                  // 值
    size_t value_len;             // 值长度
    uint32_t height;              // 层高
    struct skiplist_node* next[];  // 后继节点数组
} skiplist_node_t;

// 跳表结构
struct ppdb_skiplist {
    ppdb_sync_t sync;             // 同步机制
    skiplist_node_t* head;        // 头节点
    uint32_t max_level;           // 最大层数
    size_t size;                  // 节点数量
    size_t memory_usage;          // 内存使用量
    bool enable_hint;             // 启用查找提示
    struct {
        skiplist_node_t* last_pos;  // 上次位置
        char prefix[8];             // 前缀缓存
    } hint;
};

// 迭代器结构
struct ppdb_skiplist_iter {
    ppdb_skiplist_t* list;        // 跳表指针
    skiplist_node_t* current;     // 当前节点
};

// 生成随机层高
static uint32_t random_height(uint32_t max_level) {
    uint32_t height = 1;
    while (height < max_level && ((double)rand() / RAND_MAX) < SKIPLIST_P) {
        height++;
    }
    return height;
}

// 创建节点
static skiplist_node_t* create_node(const void* key, size_t key_len,
                                  const void* value, size_t value_len,
                                  uint32_t height) {
    size_t node_size = sizeof(skiplist_node_t) + height * sizeof(skiplist_node_t*);
    skiplist_node_t* node = (skiplist_node_t*)malloc(node_size);
    if (!node) return NULL;

    node->key = malloc(key_len);
    if (!node->key) {
        free(node);
        return NULL;
    }
    memcpy(node->key, key, key_len);
    node->key_len = key_len;

    node->value = malloc(value_len);
    if (!node->value) {
        free(node->key);
        free(node);
        return NULL;
    }
    memcpy(node->value, value, value_len);
    node->value_len = value_len;

    node->height = height;
    memset(node->next, 0, height * sizeof(skiplist_node_t*));

    return node;
}

// 销毁节点
static void destroy_node(skiplist_node_t* node) {
    if (node) {
        free(node->key);
        free(node->value);
        free(node);
    }
}

// 比较键
static int compare_key(const void* key1, size_t key1_len,
                      const void* key2, size_t key2_len) {
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result == 0) {
        if (key1_len < key2_len) return -1;
        if (key1_len > key2_len) return 1;
    }
    return result;
}

// 创建跳表
ppdb_skiplist_t* ppdb_skiplist_create(const ppdb_skiplist_config_t* config) {
    ppdb_skiplist_t* list = (ppdb_skiplist_t*)malloc(sizeof(ppdb_skiplist_t));
    if (!list) return NULL;

    if (ppdb_sync_init(&list->sync, &config->sync_config) != 0) {
        free(list);
        return NULL;
    }

    list->max_level = config->max_level > MAX_LEVEL ? MAX_LEVEL : config->max_level;
    list->size = 0;
    list->memory_usage = sizeof(ppdb_skiplist_t);
    list->enable_hint = config->enable_hint;
    memset(&list->hint, 0, sizeof(list->hint));

    // 创建头节点
    list->head = create_node(NULL, 0, NULL, 0, list->max_level);
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

    skiplist_node_t* current = list->head;
    while (current) {
        skiplist_node_t* next = current->next[0];
        destroy_node(current);
        current = next;
    }

    ppdb_sync_destroy(&list->sync);
    free(list);
}

// 插入数据
int ppdb_skiplist_insert(ppdb_skiplist_t* list, const void* key, size_t key_len,
                        const void* value, size_t value_len) {
    if (!list || !key || !value) return PPDB_ERROR;

    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current = list->head;
    
    ppdb_sync_lock(&list->sync);

    // 查找插入位置
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && 
               compare_key(current->next[i]->key, current->next[i]->key_len,
                         key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    current = current->next[0];

    // 如果key已存在，更新value
    if (current && compare_key(current->key, current->key_len, key, key_len) == 0) {
        void* new_value = malloc(value_len);
        if (!new_value) {
            ppdb_sync_unlock(&list->sync);
            return PPDB_ERROR;
        }

        memcpy(new_value, value, value_len);
        free(current->value);
        current->value = new_value;
        current->value_len = value_len;

        ppdb_sync_unlock(&list->sync);
        return PPDB_OK;
    }

    // 创建新节点
    uint32_t height = random_height(list->max_level);
    skiplist_node_t* node = create_node(key, key_len, value, value_len, height);
    if (!node) {
        ppdb_sync_unlock(&list->sync);
        return PPDB_ERROR;
    }

    // 更新指针
    for (uint32_t i = 0; i < height; i++) {
        node->next[i] = update[i]->next[i];
        update[i]->next[i] = node;
    }

    list->size++;
    list->memory_usage += sizeof(skiplist_node_t) + height * sizeof(skiplist_node_t*) +
                         key_len + value_len;

    // 更新查找提示
    if (list->enable_hint) {
        list->hint.last_pos = node;
        if (key_len >= sizeof(list->hint.prefix)) {
            memcpy(list->hint.prefix, key, sizeof(list->hint.prefix));
        } else {
            memcpy(list->hint.prefix, key, key_len);
            memset(list->hint.prefix + key_len, 0, sizeof(list->hint.prefix) - key_len);
        }
    }

    ppdb_sync_unlock(&list->sync);
    return PPDB_OK;
}

// 查找数据
int ppdb_skiplist_find(ppdb_skiplist_t* list, const void* key, size_t key_len,
                      void** value, size_t* value_len) {
    if (!list || !key || !value || !value_len) return PPDB_ERROR;

    ppdb_sync_lock(&list->sync);

    skiplist_node_t* current = list->head;

    // 使用查找提示
    if (list->enable_hint && list->hint.last_pos) {
        if (key_len >= sizeof(list->hint.prefix) &&
            memcmp(key, list->hint.prefix, sizeof(list->hint.prefix)) == 0) {
            current = list->hint.last_pos;
            while (current->next[0] && 
                   compare_key(current->next[0]->key, current->next[0]->key_len,
                             key, key_len) < 0) {
                current = current->next[0];
            }
        }
    }

    // 从高层向低层查找
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && 
               compare_key(current->next[i]->key, current->next[i]->key_len,
                         key, key_len) < 0) {
            current = current->next[i];
        }
    }

    current = current->next[0];

    // 找到节点
    if (current && compare_key(current->key, current->key_len, key, key_len) == 0) {
        *value = malloc(current->value_len);
        if (!*value) {
            ppdb_sync_unlock(&list->sync);
            return PPDB_ERROR;
        }
        memcpy(*value, current->value, current->value_len);
        *value_len = current->value_len;

        // 更新查找提示
        if (list->enable_hint) {
            list->hint.last_pos = current;
            if (key_len >= sizeof(list->hint.prefix)) {
                memcpy(list->hint.prefix, key, sizeof(list->hint.prefix));
            } else {
                memcpy(list->hint.prefix, key, key_len);
                memset(list->hint.prefix + key_len, 0, sizeof(list->hint.prefix) - key_len);
            }
        }

        ppdb_sync_unlock(&list->sync);
        return PPDB_OK;
    }

    ppdb_sync_unlock(&list->sync);
    return PPDB_NOT_FOUND;
}

// 删除数据
int ppdb_skiplist_remove(ppdb_skiplist_t* list, const void* key, size_t key_len) {
    if (!list || !key) return PPDB_ERROR;

    skiplist_node_t* update[MAX_LEVEL];
    skiplist_node_t* current = list->head;

    ppdb_sync_lock(&list->sync);

    // 查找要删除的节点
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && 
               compare_key(current->next[i]->key, current->next[i]->key_len,
                         key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    current = current->next[0];

    // 找到节点后删除
    if (current && compare_key(current->key, current->key_len, key, key_len) == 0) {
        for (uint32_t i = 0; i < current->height; i++) {
            update[i]->next[i] = current->next[i];
        }

        // 更新统计信息
        list->size--;
        list->memory_usage -= sizeof(skiplist_node_t) + 
                             current->height * sizeof(skiplist_node_t*) +
                             current->key_len + current->value_len;

        // 更新查找提示
        if (list->enable_hint && list->hint.last_pos == current) {
            list->hint.last_pos = NULL;
            memset(list->hint.prefix, 0, sizeof(list->hint.prefix));
        }

        destroy_node(current);
        ppdb_sync_unlock(&list->sync);
        return PPDB_OK;
    }

    ppdb_sync_unlock(&list->sync);
    return PPDB_NOT_FOUND;
}

// 创建迭代器
ppdb_skiplist_iter_t* ppdb_skiplist_iter_create(ppdb_skiplist_t* list) {
    if (!list) return NULL;

    ppdb_skiplist_iter_t* iter = malloc(sizeof(ppdb_skiplist_iter_t));
    if (!iter) return NULL;

    iter->list = list;
    iter->current = list->head;

    return iter;
}

// 销毁迭代器
void ppdb_skiplist_iter_destroy(ppdb_skiplist_iter_t* iter) {
    free(iter);
}

// 迭代器是否有效
bool ppdb_skiplist_iter_valid(ppdb_skiplist_iter_t* iter) {
    return iter && iter->current && iter->current->next[0];
}

// 迭代器移动到下一个
void ppdb_skiplist_iter_next(ppdb_skiplist_iter_t* iter) {
    if (iter && iter->current) {
        iter->current = iter->current->next[0];
    }
}

// 获取迭代器当前key
int ppdb_skiplist_iter_key(ppdb_skiplist_iter_t* iter, void** key, size_t* key_len) {
    if (!iter || !iter->current || !iter->current->next[0] || !key || !key_len) {
        return PPDB_ERROR;
    }

    *key = malloc(iter->current->next[0]->key_len);
    if (!*key) return PPDB_ERROR;

    memcpy(*key, iter->current->next[0]->key, iter->current->next[0]->key_len);
    *key_len = iter->current->next[0]->key_len;

    return PPDB_OK;
}

// 获取迭代器当前value
int ppdb_skiplist_iter_value(ppdb_skiplist_iter_t* iter, void** value, size_t* value_len) {
    if (!iter || !iter->current || !iter->current->next[0] || !value || !value_len) {
        return PPDB_ERROR;
    }

    *value = malloc(iter->current->next[0]->value_len);
    if (!*value) return PPDB_ERROR;

    memcpy(*value, iter->current->next[0]->value, iter->current->next[0]->value_len);
    *value_len = iter->current->next[0]->value_len;

    return PPDB_OK;
}
