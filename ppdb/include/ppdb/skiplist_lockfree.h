#ifndef PPDB_SKIPLIST_LOCKFREE_H
#define PPDB_SKIPLIST_LOCKFREE_H

#include <cosmopolitan.h>

#define MAX_LEVEL 32

// 节点状态
typedef enum {
    NODE_VALID = 0,
    NODE_DELETED = 1
} node_state_t;

// 跳表节点结构
typedef struct skiplist_node_t {
    uint8_t* key;
    size_t key_len;
    void* value;
    size_t value_len;
    uint32_t level;
    atomic_uint state;
    struct ref_count_t* ref_count;
    _Atomic(struct skiplist_node_t*) next[];
} skiplist_node_t;

// 无锁跳表结构
typedef struct atomic_skiplist_t {
    skiplist_node_t* head;
    atomic_size_t size;
    uint32_t max_level;
} atomic_skiplist_t;

// 访问者回调函数类型
typedef bool (*skiplist_visitor_t)(const uint8_t* key, size_t key_len,
                                 const uint8_t* value, size_t value_len,
                                 void* ctx);

// 创建无锁跳表
atomic_skiplist_t* atomic_skiplist_create(void);

// 销毁无锁跳表
void atomic_skiplist_destroy(atomic_skiplist_t* list);

// 插入/更新键值对
int atomic_skiplist_put(atomic_skiplist_t* list,
                       const uint8_t* key, size_t key_len,
                       const uint8_t* value, size_t value_len);

// 获取键对应的值
int atomic_skiplist_get(atomic_skiplist_t* list,
                       const uint8_t* key, size_t key_len,
                       uint8_t* value, size_t* value_len);

// 删除键值对
int atomic_skiplist_delete(atomic_skiplist_t* list,
                         const uint8_t* key, size_t key_len);

// 获取跳表大小
size_t atomic_skiplist_size(atomic_skiplist_t* list);

// 清空跳表
void atomic_skiplist_clear(atomic_skiplist_t* list);

// 遍历跳表
void atomic_skiplist_foreach(atomic_skiplist_t* list,
                           skiplist_visitor_t visitor,
                           void* ctx);

#endif // PPDB_SKIPLIST_LOCKFREE_H 