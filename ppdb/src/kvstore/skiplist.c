#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/sync.h"
#include "ppdb/kvstore/skiplist.h"

// 跳表最大层数
#define PPDB_SKIPLIST_MAX_LEVEL 32

// 比较函数类型
typedef int (*ppdb_compare_func_t)(const void* key1, size_t key1_len,
                                const void* key2, size_t key2_len);

// 跳表节点结构
typedef struct ppdb_skiplist_node {
    void* key;                  // 键
    size_t key_len;            // 键长度
    void* value;               // 值
    size_t value_len;          // 值长度
    int level;                 // 节点层数
    struct ppdb_skiplist_node** next;  // 后继节点数组
} ppdb_skiplist_node_t;

// 跳表结构
struct ppdb_skiplist {
    ppdb_skiplist_node_t* head;  // 头节点
    int level;                   // 当前最大层数
    int max_level;               // 最大层数
    size_t size;                 // 节点数量
    size_t memory_usage;         // 内存使用量
    ppdb_sync_t* sync;           // 同步原语
    ppdb_compare_func_t compare; // 比较函数
};

// 迭代器结构
struct ppdb_skiplist_iterator {
    ppdb_skiplist_t* list;       // 关联的跳表
    ppdb_skiplist_node_t* current;  // 当前节点
    bool valid;                  // 迭代器是否有效
    ppdb_kv_pair_t current_pair;  // 当前键值对的缓存
    ppdb_sync_t* sync;           // 迭代器的同步原语
};

// 内部函数声明
static ppdb_skiplist_node_t* create_node(const void* key, size_t key_len,
                                      const void* value, size_t value_len,
                                      int level);
static void destroy_node(ppdb_skiplist_node_t* node);
static int random_level(int max_level);
static ppdb_error_t skiplist_put_internal(ppdb_skiplist_t* list,
                                        const void* key, size_t key_len,
                                        const void* value, size_t value_len);
ppdb_error_t skiplist_get_internal(ppdb_skiplist_t* list,
                                 const void* key, size_t key_len,
                                 void** value, size_t* value_len);
static ppdb_error_t skiplist_delete_internal(ppdb_skiplist_t* list,
                                           const void* key, size_t key_len);

// 生成随机层数
static int random_level(int max_level) {
    if (max_level <= 0 || max_level > PPDB_SKIPLIST_MAX_LEVEL) {
        return 1;  // 返回最小有效层数
    }

    int level = 1;
    while (level < max_level && (rand() & 0x1)) {
        level++;
    }
    return level;
}

// 创建跳表
ppdb_error_t ppdb_skiplist_create(ppdb_skiplist_t** list, int max_level,
                                 ppdb_compare_func_t compare,
                                 const ppdb_sync_config_t* sync_config) {
    printf("ppdb_skiplist_create: Starting with max_level=%d\n", max_level);
    
    if (!list || !compare || !sync_config || max_level <= 0 || max_level > PPDB_SKIPLIST_MAX_LEVEL) {
        printf("ppdb_skiplist_create: Invalid parameters\n");
        return PPDB_ERR_INVALID_PARAM;
    }

    // 分配并初始化跳表结构
    ppdb_skiplist_t* new_list = calloc(1, sizeof(ppdb_skiplist_t));
    if (!new_list) {
        printf("ppdb_skiplist_create: Failed to allocate skiplist\n");
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化头节点
    ppdb_skiplist_node_t* head = calloc(1, sizeof(ppdb_skiplist_node_t));
    if (!head) {
        printf("ppdb_skiplist_create: Failed to allocate head node\n");
        free(new_list);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    head->next = calloc(max_level, sizeof(ppdb_skiplist_node_t*));
    if (!head->next) {
        printf("ppdb_skiplist_create: Failed to allocate next array\n");
        free(head);
        free(new_list);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化跳表
    new_list->head = head;
    new_list->max_level = max_level;
    new_list->level = 1;
    new_list->size = 0;
    new_list->memory_usage = sizeof(ppdb_skiplist_t) + sizeof(ppdb_skiplist_node_t) +
                            max_level * sizeof(ppdb_skiplist_node_t*);
    new_list->compare = compare;

    // 初始化同步原语
    new_list->sync = malloc(sizeof(ppdb_sync_t));
    if (!new_list->sync) {
        printf("ppdb_skiplist_create: Failed to allocate sync\n");
        free(head->next);
        free(head);
        free(new_list);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    printf("ppdb_skiplist_create: Initializing sync\n");
    ppdb_error_t err = ppdb_sync_init(new_list->sync, sync_config);
    if (err != PPDB_OK) {
        printf("ppdb_skiplist_create: Failed to init sync: %d\n", err);
        free(new_list->sync);
        free(head->next);
        free(head);
        free(new_list);
        return err;
    }

    *list = new_list;
    printf("ppdb_skiplist_create: Successfully created skiplist\n");
    return PPDB_OK;
}

// 插入键值对
ppdb_error_t ppdb_skiplist_put(ppdb_skiplist_t* list, const void* key, size_t key_len,
                              const void* value, size_t value_len) {
    printf("ppdb_skiplist_put: key_len=%zu, value_len=%zu\n", key_len, value_len);
    
    if (!list || !key || !value || key_len == 0 || value_len == 0) {
        printf("ppdb_skiplist_put: Invalid parameters - list=%p, key=%p, value=%p, key_len=%zu, value_len=%zu\n",
               list, key, value, key_len, value_len);
        return PPDB_ERR_INVALID_PARAM;
    }

    printf("ppdb_skiplist_put: Acquiring write lock\n");
    // 获取写锁
    ppdb_error_t err = ppdb_sync_write_lock(list->sync);
    if (err != PPDB_OK) {
        printf("ppdb_skiplist_put: Failed to acquire write lock: %d\n", err);
        return err;
    }

    printf("ppdb_skiplist_put: Calling skiplist_put_internal\n");
    // 执行插入
    err = skiplist_put_internal(list, key, key_len, value, value_len);
    printf("ppdb_skiplist_put: skiplist_put_internal returned: %d\n", err);

    printf("ppdb_skiplist_put: Releasing write lock\n");
    // 释放写锁
    ppdb_sync_write_unlock(list->sync);
    
    printf("ppdb_skiplist_put: Returning %d\n", err);
    return err;
}

// 内部插入实现
static ppdb_error_t skiplist_put_internal(ppdb_skiplist_t* list,
                                        const void* key, size_t key_len,
                                        const void* value, size_t value_len) {
    printf("skiplist_put_internal: key_len=%zu, value_len=%zu\n", key_len, value_len);
    
    if (!list || !key || !value || key_len == 0 || value_len == 0) {
        printf("skiplist_put_internal: Invalid parameters - list=%p, key=%p, value=%p, key_len=%zu, value_len=%zu\n",
               list, key, value, key_len, value_len);
        return PPDB_ERR_INVALID_PARAM;
    }

    // 查找插入位置
    ppdb_skiplist_node_t* update[PPDB_SKIPLIST_MAX_LEVEL] = {0};
    ppdb_skiplist_node_t* current = list->head;

    printf("skiplist_put_internal: Starting search from level %d\n", list->level - 1);
    // 从最高层开始查找
    for (int i = list->level - 1; i >= 0; i--) {
        printf("skiplist_put_internal: Searching at level %d\n", i);
        while (current->next[i] && list->compare(current->next[i]->key,
                                               current->next[i]->key_len,
                                               key, key_len) < 0) {
            current = current->next[i];
            printf("skiplist_put_internal: Moving to next node at level %d\n", i);
        }
        update[i] = current;
    }

    current = current->next[0];

    // 检查是否已存在相同的键
    if (current && list->compare(current->key, current->key_len,
                                key, key_len) == 0) {
        printf("skiplist_put_internal: Updating existing key\n");
        // 更新值
        void* new_value = malloc(value_len);
        if (!new_value) {
            printf("skiplist_put_internal: Failed to allocate memory for new value\n");
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memcpy(new_value, value, value_len);
        
        // 更新内存使用统计
        list->memory_usage = list->memory_usage - current->value_len + value_len;
        
        // 释放旧值并更新新值
        void* old_value = current->value;
        current->value = new_value;
        current->value_len = value_len;
        free(old_value);
        
        printf("skiplist_put_internal: Successfully updated existing key\n");
        return PPDB_OK;
    }

    // 生成随机层数
    int level = random_level(list->max_level);
    printf("skiplist_put_internal: Generated random level: %d\n", level);
    if (level <= 0 || level > list->max_level) {
        printf("skiplist_put_internal: Invalid level generated: %d\n", level);
        return PPDB_ERR_INTERNAL;
    }

    // 创建新节点
    ppdb_skiplist_node_t* new_node = create_node(key, key_len, value, value_len, level);
    if (!new_node) {
        printf("skiplist_put_internal: Failed to create new node\n");
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    printf("skiplist_put_internal: Inserting new node at level %d\n", level);
    // 插入节点
    for (int i = 0; i < level; i++) {
        printf("skiplist_put_internal: Inserting at level %d\n", i);
        if (i >= list->level) {
            // 如果新节点的层数超过当前最大层数，将头节点作为前驱
            printf("skiplist_put_internal: Extending level from %d to %d\n", list->level, i + 1);
            update[i] = list->head;
            list->level = i + 1;
        }
        new_node->next[i] = update[i]->next[i];
        update[i]->next[i] = new_node;
        printf("skiplist_put_internal: Successfully inserted at level %d\n", i);
    }

    // 更新统计信息
    list->size++;
    list->memory_usage += sizeof(ppdb_skiplist_node_t) + key_len + value_len +
                         level * sizeof(ppdb_skiplist_node_t*);

    printf("skiplist_put_internal: Successfully inserted new key\n");
    return PPDB_OK;
}

// 销毁跳表
void ppdb_skiplist_destroy(ppdb_skiplist_t* list) {
    if (!list) {
        return;
    }

    // 获取写锁
    ppdb_error_t err = ppdb_sync_write_lock(list->sync);
    if (err != PPDB_OK) {
        return;
    }

    // 清理所有节点
    ppdb_skiplist_node_t* current = list->head;
    while (current) {
        ppdb_skiplist_node_t* next = current->next[0];
        destroy_node(current);
        current = next;
    }

    // 销毁同步原语
    ppdb_sync_write_unlock(list->sync);
    ppdb_sync_destroy(list->sync);
    free(list->sync);

    // 释放跳表结构
    free(list);
}

// 创建节点
static ppdb_skiplist_node_t* create_node(const void* key, size_t key_len,
                                      const void* value, size_t value_len,
                                      int level) {
    printf("create_node: level=%d, key_len=%zu, value_len=%zu\n", level, key_len, value_len);
    printf("create_node: key='%.*s', value='%.*s'\n", 
           (int)key_len, (const char*)key,
           (int)value_len, (const char*)value);
    
    if (level <= 0 || level > PPDB_SKIPLIST_MAX_LEVEL) {
        printf("create_node: Invalid level\n");
        return NULL;
    }

    // 分配节点内存并初始化为0
    ppdb_skiplist_node_t* node = calloc(1, sizeof(ppdb_skiplist_node_t));
    if (!node) {
        printf("create_node: Failed to allocate node\n");
        return NULL;
    }

    // 分配 next 数组
    node->next = calloc(level, sizeof(ppdb_skiplist_node_t*));
    if (!node->next) {
        printf("create_node: Failed to allocate next array\n");
        free(node);
        return NULL;
    }

    // 分配并复制键
    if (key && key_len > 0) {
        node->key = malloc(key_len);
        if (!node->key) {
            printf("create_node: Failed to allocate key\n");
            free(node->next);
            free(node);
            return NULL;
        }
        memcpy(node->key, key, key_len);
        node->key_len = key_len;
        printf("create_node: Successfully allocated and copied key\n");
    } else {
        printf("create_node: Invalid key parameters\n");
        free(node->next);
        free(node);
        return NULL;
    }

    // 分配并复制值
    if (value && value_len > 0) {
        node->value = malloc(value_len);
        if (!node->value) {
            printf("create_node: Failed to allocate value\n");
            free(node->key);
            free(node->next);
            free(node);
            return NULL;
        }
        memcpy(node->value, value, value_len);
        node->value_len = value_len;
        printf("create_node: Successfully allocated and copied value\n");
    } else {
        printf("create_node: Invalid value parameters\n");
        free(node->key);
        free(node->next);
        free(node);
        return NULL;
    }

    node->level = level;
    printf("create_node: Successfully created node with key='%.*s', value='%.*s'\n",
           (int)node->key_len, (const char*)node->key,
           (int)node->value_len, (const char*)node->value);
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

// 获取键值对
ppdb_error_t ppdb_skiplist_get(ppdb_skiplist_t* list,
                             const void* key, size_t key_len,
                             void** value, size_t* value_len) {
    printf("ppdb_skiplist_get: Starting with key='%.*s' (len=%zu), value=%p, value_len=%p\n",
           (int)key_len, (const char*)key, key_len, (void*)value, (void*)value_len);
    
    if (!list || !key || !value_len) {
        printf("ppdb_skiplist_get: Invalid parameters - list=%p, key=%p, value_len=%p\n",
               (void*)list, key, (void*)value_len);
        return PPDB_ERR_INVALID_PARAM;
    }

    // 获取读锁
    ppdb_error_t err = ppdb_sync_read_lock(list->sync);
    if (err != PPDB_OK) {
        printf("ppdb_skiplist_get: Failed to acquire read lock: %d\n", err);
        return err;
    }

    // 执行查找
    err = skiplist_get_internal(list, key, key_len, value, value_len);
    printf("ppdb_skiplist_get: skiplist_get_internal returned %d, value_len=%zu\n",
           err, *value_len);

    // 释放读锁
    ppdb_sync_read_unlock(list->sync);
    printf("ppdb_skiplist_get: Returning %d\n", err);
    return err;
}

// 内部查找实现
ppdb_error_t skiplist_get_internal(ppdb_skiplist_t* list,
                                 const void* key, size_t key_len,
                                 void** value, size_t* value_len) {
    printf("skiplist_get_internal: Starting with key='%.*s' (len=%zu), value=%p, value_len=%p\n",
           (int)(key_len - 1), (const char*)key, key_len, (void*)value, (void*)value_len);
    
    if (!list || !key || !value_len) {
        printf("skiplist_get_internal: Invalid parameters - list=%p, key=%p, value_len=%p\n",
               (void*)list, key, (void*)value_len);
        return PPDB_ERR_INVALID_PARAM;
    }

    ppdb_skiplist_node_t* current = list->head;
    printf("skiplist_get_internal: Starting search from head node\n");

    // 从最高层开始查找
    for (int i = list->level - 1; i >= 0; i--) {
        printf("skiplist_get_internal: Searching at level %d\n", i);
        while (current->next[i] && list->compare(current->next[i]->key,
                                               current->next[i]->key_len,
                                               key, key_len) < 0) {
            current = current->next[i];
            printf("skiplist_get_internal: Moving to next node at level %d\n", i);
        }
    }

    // 移动到下一个节点
    current = current->next[0];

    // 检查是否找到
    if (!current) {
        printf("skiplist_get_internal: Node not found (current is NULL)\n");
        *value_len = 0;
        return PPDB_ERR_NOT_FOUND;
    }

    // 比较键
    int cmp = list->compare(current->key, current->key_len, key, key_len);
    printf("skiplist_get_internal: Compare result=%d (current_key='%.*s', key='%.*s')\n", 
           cmp, (int)(current->key_len - 1), (const char*)current->key,
           (int)(key_len - 1), (const char*)key);

    if (cmp != 0) {
        printf("skiplist_get_internal: Key mismatch\n");
        *value_len = 0;
        return PPDB_ERR_NOT_FOUND;
    }

    printf("skiplist_get_internal: Found key, value_len=%zu\n", current->value_len);
    *value_len = current->value_len;

    // 如果只需要获取大小
    if (!value) {
        printf("skiplist_get_internal: Only size requested, returning OK\n");
        return PPDB_OK;
    }

    // 分配并复制值
    void* new_value = malloc(current->value_len);
    if (!new_value) {
        printf("skiplist_get_internal: Failed to allocate memory for value\n");
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    memcpy(new_value, current->value, current->value_len);
    *value = new_value;

    printf("skiplist_get_internal: Successfully copied value\n");
    return PPDB_OK;
}

// 删除键值对
ppdb_error_t ppdb_skiplist_delete(ppdb_skiplist_t* list,
                                const void* key, size_t key_len) {
    if (!list || !key) {
        return PPDB_ERR_INVALID_PARAM;
    }

    // 获取写锁
    ppdb_error_t err = ppdb_sync_write_lock(list->sync);
    if (err != PPDB_OK) {
        return err;
    }

    // 执行删除
    err = skiplist_delete_internal(list, key, key_len);

    // 释放写锁
    ppdb_sync_write_unlock(list->sync);
    return err;
}

// 内部删除实现
static ppdb_error_t skiplist_delete_internal(ppdb_skiplist_t* list,
                                          const void* key, size_t key_len) {
    ppdb_skiplist_node_t* update[PPDB_SKIPLIST_MAX_LEVEL];
    ppdb_skiplist_node_t* current = list->head;

    // 从最高层开始查找
    for (int i = list->max_level - 1; i >= 0; i--) {
        while (current->next[i] && list->compare(current->next[i]->key,
                                               current->next[i]->key_len,
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

    // 更新所有层的指针
    for (int i = 0; i < list->max_level; i++) {
        if (update[i]->next[i] != current) {
            break;
        }
        update[i]->next[i] = current->next[i];
    }

    // 更新统计信息
    list->size--;
    list->memory_usage -= sizeof(ppdb_skiplist_node_t) +
                         current->level * sizeof(ppdb_skiplist_node_t*) +
                         current->key_len + current->value_len;

    // 释放节点
    if (current->key) free(current->key);
    if (current->value) free(current->value);
    if (current->next) free(current->next);
    free(current);

    return PPDB_OK;
}

// 统计信息相关函数
size_t ppdb_skiplist_size(const ppdb_skiplist_t* list) {
    if (!list) return 0;
    return list->size;
}

size_t ppdb_skiplist_memory_usage(const ppdb_skiplist_t* list) {
    if (!list) return 0;
    return list->memory_usage;
}

bool ppdb_skiplist_empty(const ppdb_skiplist_t* list) {
    if (!list) return true;
    return list->size == 0;
}

// 清空跳表
void ppdb_skiplist_clear(ppdb_skiplist_t* list) {
    if (!list) return;

    // 获取写锁
    ppdb_error_t err = ppdb_sync_write_lock(list->sync);
    if (err != PPDB_OK) {
        return;
    }

    // 清理所有节点（除了头节点）
    ppdb_skiplist_node_t* current = list->head->next[0];
    while (current) {
        ppdb_skiplist_node_t* next = current->next[0];
        if (current->key) free(current->key);
        if (current->value) free(current->value);
        if (current->next) free(current->next);
        free(current);
        current = next;
    }

    // 重置头节点的所有层
    for (int i = 0; i < list->max_level; i++) {
        list->head->next[i] = NULL;
    }

    // 重置统计信息
    list->size = 0;
    list->memory_usage = sizeof(ppdb_skiplist_t) + sizeof(ppdb_skiplist_node_t) +
                        list->max_level * sizeof(ppdb_skiplist_node_t*);

    // 释放写锁
    ppdb_sync_write_unlock(list->sync);
}

// 创建迭代器
ppdb_error_t ppdb_skiplist_iterator_create(ppdb_skiplist_t* list,
                                        ppdb_skiplist_iterator_t** iter,
                                        const ppdb_sync_config_t* sync_config) {
    printf("ppdb_skiplist_iterator_create: Starting\n");
    if (!list || !iter || !sync_config) {
        printf("ppdb_skiplist_iterator_create: Invalid parameters\n");
        return PPDB_ERR_INVALID_PARAM;
    }

    // 分配迭代器结构
    ppdb_skiplist_iterator_t* new_iter = calloc(1, sizeof(ppdb_skiplist_iterator_t));
    if (!new_iter) {
        printf("ppdb_skiplist_iterator_create: Failed to allocate iterator\n");
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化迭代器
    new_iter->list = list;
    new_iter->current = list->head;  // 从头节点开始
    new_iter->valid = true;  // 初始状态为有效
    new_iter->current_pair.key = NULL;
    new_iter->current_pair.value = NULL;

    // 分配同步原语
    new_iter->sync = malloc(sizeof(ppdb_sync_t));
    if (!new_iter->sync) {
        printf("ppdb_skiplist_iterator_create: Failed to allocate sync\n");
        free(new_iter);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化同步原语
    ppdb_error_t err = ppdb_sync_init(new_iter->sync, sync_config);
    if (err != PPDB_OK) {
        printf("ppdb_skiplist_iterator_create: Failed to init sync: %d\n", err);
        free(new_iter->sync);
        free(new_iter);
        return err;
    }

    // 获取迭代器的写锁
    err = ppdb_sync_write_lock(new_iter->sync);
    if (err != PPDB_OK) {
        printf("ppdb_skiplist_iterator_create: Failed to acquire write lock: %d\n", err);
        ppdb_sync_destroy(new_iter->sync);
        free(new_iter->sync);
        free(new_iter);
        return err;
    }

    // 获取跳表的读锁
    err = ppdb_sync_read_lock(list->sync);
    if (err != PPDB_OK) {
        printf("ppdb_skiplist_iterator_create: Failed to acquire read lock: %d\n", err);
        ppdb_sync_write_unlock(new_iter->sync);
        ppdb_sync_destroy(new_iter->sync);
        free(new_iter->sync);
        free(new_iter);
        return err;
    }

    // 移动到第一个有效节点
    if (new_iter->current && new_iter->current->next[0]) {
        // 复制第一个节点的数据
        ppdb_skiplist_node_t* first = new_iter->current->next[0];
        new_iter->current_pair.key = malloc(first->key_len);
        if (!new_iter->current_pair.key) {
            printf("ppdb_skiplist_iterator_create: Failed to allocate key\n");
            ppdb_sync_read_unlock(list->sync);
            ppdb_sync_write_unlock(new_iter->sync);
            ppdb_sync_destroy(new_iter->sync);
            free(new_iter->sync);
            free(new_iter);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memcpy(new_iter->current_pair.key, first->key, first->key_len);
        new_iter->current_pair.key_size = first->key_len;

        new_iter->current_pair.value = malloc(first->value_len);
        if (!new_iter->current_pair.value) {
            printf("ppdb_skiplist_iterator_create: Failed to allocate value\n");
            free(new_iter->current_pair.key);
            ppdb_sync_read_unlock(list->sync);
            ppdb_sync_write_unlock(new_iter->sync);
            ppdb_sync_destroy(new_iter->sync);
            free(new_iter->sync);
            free(new_iter);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memcpy(new_iter->current_pair.value, first->value, first->value_len);
        new_iter->current_pair.value_size = first->value_len;
        new_iter->valid = true;
        new_iter->current = first;
    } else {
        new_iter->valid = false;
    }

    *iter = new_iter;

    // 释放跳表的读锁
    ppdb_sync_read_unlock(list->sync);
    // 释放迭代器的写锁
    ppdb_sync_write_unlock(new_iter->sync);

    printf("ppdb_skiplist_iterator_create: Successfully created iterator\n");
    return PPDB_OK;
}

// 销毁迭代器
void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* iter) {
    if (!iter) return;

    // 获取迭代器的写锁
    ppdb_error_t err = ppdb_sync_write_lock(iter->sync);
    if (err != PPDB_OK) {
        return;
    }

    // 获取跳表的读锁
    err = ppdb_sync_read_lock(iter->list->sync);
    if (err != PPDB_OK) {
        ppdb_sync_write_unlock(iter->sync);
        return;
    }

    // 释放当前键值对
    if (iter->current_pair.key) {
        free(iter->current_pair.key);
        iter->current_pair.key = NULL;
    }
    if (iter->current_pair.value) {
        free(iter->current_pair.value);
        iter->current_pair.value = NULL;
    }

    // 释放跳表的读锁
    ppdb_sync_read_unlock(iter->list->sync);

    // 销毁同步原语
    if (iter->sync) {
        // 释放迭代器的写锁
        ppdb_sync_write_unlock(iter->sync);
        ppdb_sync_destroy(iter->sync);
        free(iter->sync);
        iter->sync = NULL;
    }

    // 释放迭代器结构
    free(iter);
}

// 移动迭代器到下一个位置
ppdb_error_t ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* iter) {
    if (!iter) return PPDB_ERR_INVALID_PARAM;

    // 获取迭代器的写锁
    ppdb_error_t err = ppdb_sync_write_lock(iter->sync);
    if (err != PPDB_OK) {
        return err;
    }

    // 获取跳表的读锁
    err = ppdb_sync_read_lock(iter->list->sync);
    if (err != PPDB_OK) {
        ppdb_sync_write_unlock(iter->sync);
        return err;
    }

    // 如果当前节点无效或已到末尾
    if (!iter->current || !iter->valid || !iter->current->next[0]) {
        iter->valid = false;
        ppdb_sync_read_unlock(iter->list->sync);
        ppdb_sync_write_unlock(iter->sync);
        return PPDB_ERR_NOT_FOUND;
    }

    // 移动到下一个节点
    ppdb_skiplist_node_t* next = iter->current->next[0];
    if (!next) {
        iter->valid = false;
        ppdb_sync_read_unlock(iter->list->sync);
        ppdb_sync_write_unlock(iter->sync);
        return PPDB_ERR_NOT_FOUND;
    }

    // 释放当前键值对的内存
    if (iter->current_pair.key) {
        free(iter->current_pair.key);
        iter->current_pair.key = NULL;
    }
    if (iter->current_pair.value) {
        free(iter->current_pair.value);
        iter->current_pair.value = NULL;
    }

    // 复制下一个节点的数据
    iter->current_pair.key = malloc(next->key_len);
    if (!iter->current_pair.key) {
        iter->valid = false;
        ppdb_sync_read_unlock(iter->list->sync);
        ppdb_sync_write_unlock(iter->sync);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(iter->current_pair.key, next->key, next->key_len);
    iter->current_pair.key_size = next->key_len;

    iter->current_pair.value = malloc(next->value_len);
    if (!iter->current_pair.value) {
        free(iter->current_pair.key);
        iter->current_pair.key = NULL;
        iter->valid = false;
        ppdb_sync_read_unlock(iter->list->sync);
        ppdb_sync_write_unlock(iter->sync);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    memcpy(iter->current_pair.value, next->value, next->value_len);
    iter->current_pair.value_size = next->value_len;

    iter->current = next;
    iter->valid = true;

    // 释放锁
    ppdb_sync_read_unlock(iter->list->sync);
    ppdb_sync_write_unlock(iter->sync);
    return PPDB_OK;
}

// 获取迭代器当前键值对
ppdb_error_t ppdb_skiplist_iterator_get(ppdb_skiplist_iterator_t* iter,
                                      ppdb_kv_pair_t* pair) {
    if (!iter || !pair) return PPDB_ERR_INVALID_PARAM;

    // 获取迭代器的读锁
    ppdb_error_t err = ppdb_sync_read_lock(iter->sync);
    if (err != PPDB_OK) {
        return err;
    }

    // 获取跳表的读锁
    err = ppdb_sync_read_lock(iter->list->sync);
    if (err != PPDB_OK) {
        ppdb_sync_read_unlock(iter->sync);
        return err;
    }

    if (!iter->current || !iter->valid || !iter->current_pair.key || !iter->current_pair.value) {
        err = PPDB_ERR_NOT_FOUND;
        goto unlock;
    }

    // 复制当前缓存的键值对
    pair->key = malloc(iter->current_pair.key_size);
    if (!pair->key) {
        err = PPDB_ERR_OUT_OF_MEMORY;
        goto unlock;
    }
    memcpy(pair->key, iter->current_pair.key, iter->current_pair.key_size);
    pair->key_size = iter->current_pair.key_size;

    pair->value = malloc(iter->current_pair.value_size);
    if (!pair->value) {
        free(pair->key);
        pair->key = NULL;
        err = PPDB_ERR_OUT_OF_MEMORY;
        goto unlock;
    }
    memcpy(pair->value, iter->current_pair.value, iter->current_pair.value_size);
    pair->value_size = iter->current_pair.value_size;

unlock:
    // 释放跳表的读锁
    ppdb_sync_read_unlock(iter->list->sync);
    // 释放迭代器的读锁
    ppdb_sync_read_unlock(iter->sync);
    return err;
}

// 检查迭代器是否有效
bool ppdb_skiplist_iterator_valid(const ppdb_skiplist_iterator_t* iter) {
    if (!iter) return false;

    // 获取迭代器的读锁
    ppdb_error_t err = ppdb_sync_read_lock(iter->sync);
    if (err != PPDB_OK) {
        return false;
    }

    bool valid = iter->valid && iter->current != NULL &&
                iter->current_pair.key != NULL && iter->current_pair.value != NULL;

    // 释放迭代器的读锁
    ppdb_sync_read_unlock(iter->sync);

    return valid;
}

// 默认的比较函数
int ppdb_skiplist_default_compare(const void* key1, size_t key1_len,
                                const void* key2, size_t key2_len) {
    printf("ppdb_skiplist_default_compare: key1='%.*s' (len=%zu), key2='%.*s' (len=%zu)\n",
           (int)key1_len, (const char*)key1, key1_len,
           (int)key2_len, (const char*)key2, key2_len);
    
    if (!key1 || !key2) {
        printf("ppdb_skiplist_default_compare: NULL key detected - key1=%p, key2=%p\n",
               key1, key2);
        return key1 ? 1 : (key2 ? -1 : 0);
    }
    if (key1_len == 0 || key2_len == 0) {
        printf("ppdb_skiplist_default_compare: Zero length key detected - key1_len=%zu, key2_len=%zu\n",
               key1_len, key2_len);
        return key1_len ? 1 : (key2_len ? -1 : 0);
    }
    size_t min_len = key1_len < key2_len ? key1_len : key2_len;
    int result = memcmp(key1, key2, min_len);
    printf("ppdb_skiplist_default_compare: memcmp result=%d\n", result);
    if (result != 0) {
        return result;
    }
    printf("ppdb_skiplist_default_compare: memcmp equal, comparing lengths\n");
    return (key1_len < key2_len) ? -1 : (key1_len > key2_len ? 1 : 0);
}
