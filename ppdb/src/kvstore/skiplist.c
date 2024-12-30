#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/skiplist.h"
#include "internal/sync.h"
#include "internal/metrics.h"

// 跳表参数
#define SKIPLIST_MAX_LEVEL 12
#define SKIPLIST_P 0.25

// 节点结构 - 缓存行对齐优化
typedef struct skiplist_node {
    ppdb_sync_t sync;              // 同步原语
    void* key;                     // 键
    size_t key_len;               // 键长度
    void* value;                  // 值
    size_t value_len;             // 值长度
    uint32_t height;              // 高度
    struct skiplist_node* next[0]; // 后继节点数组
} __attribute__((aligned(64))) skiplist_node_t;

// 跳表结构
struct ppdb_skiplist {
    ppdb_sync_t sync;             // 同步原语
    skiplist_node_t* head;        // 头节点
    uint32_t max_level;           // 最大层数
    _Atomic(size_t) size;         // 节点数量
    _Atomic(size_t) memory_usage; // 内存使用
    bool enable_hint;             // 启用搜索提示
    struct {
        skiplist_node_t* last_pos;  // 上次位置
        char prefix[8];             // 前缀缓存
    } hint;
};

// 生成随机高度
static uint32_t random_height(void) {
    uint32_t height = 1;
    while (height < SKIPLIST_MAX_LEVEL && ((double)rand() / RAND_MAX) < SKIPLIST_P) {
        height++;
    }
    return height;
}

// 创建节点
static skiplist_node_t* create_node(const uint8_t* key, size_t key_len,
                                  const uint8_t* value, size_t value_len,
                                  uint32_t height) {
    // 计算总大小,包括填充以对齐缓存行
    size_t node_size = sizeof(skiplist_node_t) + height * sizeof(skiplist_node_t*);
    node_size = (node_size + 63) & ~63;  // 向上取整到64字节

    // 分配对齐的内存
    skiplist_node_t* node = aligned_alloc(64, node_size);
    if (!node) return NULL;

    // 初始化同步原语
    ppdb_sync_config_t sync_config = {
        .use_lockfree = false,
        .stripe_count = 0,
        .spin_count = 1000,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    ppdb_sync_init(&node->sync, &sync_config);
    
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
        node->next[i] = NULL;
    }
    
    return node;
}

// 销毁节点
static void destroy_node(skiplist_node_t* node) {
    if (!node) return;
    ppdb_sync_destroy(&node->sync);
    free(node->key);
    free(node->value);
    free(node);
}

// 比较键
static int compare_key(const uint8_t* key1, size_t key1_len,
                      const uint8_t* key2, size_t key2_len) {
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    if (result != 0) return result;
    return key1_len - key2_len;
}

// 创建跳表
ppdb_error_t ppdb_skiplist_create(ppdb_skiplist_t** list) {
    if (!list) return PPDB_ERR_NULL_POINTER;

    ppdb_skiplist_t* new_list = aligned_alloc(64, sizeof(ppdb_skiplist_t));
    if (!new_list) return PPDB_ERR_NO_MEMORY;

    // 初始化同步原语
    ppdb_sync_config_t sync_config = {
        .use_lockfree = false,
        .stripe_count = 0,
        .spin_count = 1000,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    ppdb_sync_init(&new_list->sync, &sync_config);

    // 初始化基本字段
    new_list->max_level = SKIPLIST_MAX_LEVEL;
    atomic_init(&new_list->size, 0);
    atomic_init(&new_list->memory_usage, sizeof(ppdb_skiplist_t));
    new_list->enable_hint = false;
    new_list->hint.last_pos = NULL;
    memset(new_list->hint.prefix, 0, sizeof(new_list->hint.prefix));

    // 创建头节点
    new_list->head = create_node(NULL, 0, NULL, 0, SKIPLIST_MAX_LEVEL);
    if (!new_list->head) {
        ppdb_sync_destroy(&new_list->sync);
        free(new_list);
        return PPDB_ERR_NO_MEMORY;
    }

    *list = new_list;
    return PPDB_OK;
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

// 插入键值对
ppdb_error_t ppdb_skiplist_put(ppdb_skiplist_t* list,
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len) {
    if (!list || !key || !value) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0 || value_len == 0) return PPDB_ERR_INVALID_ARG;

    ppdb_sync_lock(&list->sync);

    // 查找插入位置
    skiplist_node_t* update[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* current = list->head;

    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && compare_key(current->next[i]->key, current->next[i]->key_len,
                                             key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    // 检查是否已存在
    current = current->next[0];
    if (current && compare_key(current->key, current->key_len, key, key_len) == 0) {
        // 更新值
        void* new_value = malloc(value_len);
        if (!new_value) {
            ppdb_sync_unlock(&list->sync);
            return PPDB_ERR_NO_MEMORY;
        }

        void* old_value = current->value;
        size_t old_size = current->value_len;

        memcpy(new_value, value, value_len);
        current->value = new_value;
        current->value_len = value_len;

        atomic_fetch_add(&list->memory_usage, (ssize_t)value_len - (ssize_t)old_size);

        ppdb_sync_unlock(&list->sync);
        free(old_value);
        return PPDB_OK;
    }

    // 创建新节点
    uint32_t height = random_height();
    skiplist_node_t* node = create_node(key, key_len, value, value_len, height);
    if (!node) {
        ppdb_sync_unlock(&list->sync);
        return PPDB_ERR_NO_MEMORY;
    }

    // 插入节点
    for (uint32_t i = 0; i < height; i++) {
        node->next[i] = update[i]->next[i];
        update[i]->next[i] = node;
    }

    atomic_fetch_add(&list->size, 1);
    atomic_fetch_add(&list->memory_usage, sizeof(skiplist_node_t) + height * sizeof(skiplist_node_t*) + key_len + value_len);

    ppdb_sync_unlock(&list->sync);
    return PPDB_OK;
}

// 获取键值对
ppdb_error_t ppdb_skiplist_get(ppdb_skiplist_t* list,
                              const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len) {
    if (!list || !key || !value || !value_len) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

    ppdb_sync_lock(&list->sync);

    // 查找节点
    skiplist_node_t* current = list->head;
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && compare_key(current->next[i]->key, current->next[i]->key_len,
                                             key, key_len) < 0) {
            current = current->next[i];
        }
    }

    current = current->next[0];
    if (current && compare_key(current->key, current->key_len, key, key_len) == 0) {
        // 复制值
        *value = malloc(current->value_len);
        if (!*value) {
            ppdb_sync_unlock(&list->sync);
            return PPDB_ERR_NO_MEMORY;
        }
        memcpy(*value, current->value, current->value_len);
        *value_len = current->value_len;
        ppdb_sync_unlock(&list->sync);
        return PPDB_OK;
    }

    ppdb_sync_unlock(&list->sync);
    return PPDB_ERR_NOT_FOUND;
}

// 删除键值对
ppdb_error_t ppdb_skiplist_delete(ppdb_skiplist_t* list,
                                 const uint8_t* key, size_t key_len) {
    if (!list || !key) return PPDB_ERR_NULL_POINTER;
    if (key_len == 0) return PPDB_ERR_INVALID_ARG;

    ppdb_sync_lock(&list->sync);

    // 查找删除位置
    skiplist_node_t* update[SKIPLIST_MAX_LEVEL];
    skiplist_node_t* current = list->head;

    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && compare_key(current->next[i]->key, current->next[i]->key_len,
                                             key, key_len) < 0) {
            current = current->next[i];
        }
        update[i] = current;
    }

    current = current->next[0];
    if (!current || compare_key(current->key, current->key_len, key, key_len) != 0) {
        ppdb_sync_unlock(&list->sync);
        return PPDB_ERR_NOT_FOUND;
    }

    // 更新指针
    for (uint32_t i = 0; i < current->height; i++) {
        if (update[i]->next[i] != current) break;
        update[i]->next[i] = current->next[i];
    }

    atomic_fetch_sub(&list->size, 1);
    atomic_fetch_sub(&list->memory_usage, sizeof(skiplist_node_t) + current->height * sizeof(skiplist_node_t*) + current->key_len + current->value_len);

    destroy_node(current);
    ppdb_sync_unlock(&list->sync);
    return PPDB_OK;
}

// 获取大小
size_t ppdb_skiplist_size(ppdb_skiplist_t* list) {
    if (!list) return 0;
    return atomic_load(&list->size);
}
