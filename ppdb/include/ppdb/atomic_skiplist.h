#ifndef PPDB_ATOMIC_SKIPLIST_H
#define PPDB_ATOMIC_SKIPLIST_H

#include <cosmopolitan.h>
#include "ppdb/ref_count.h"

// 最大层数
#define MAX_LEVEL 32

// 跳表节点状态
typedef enum {
    NODE_VALID = 0,    // 节点有效
    NODE_DELETED = 1,  // 节点已删除
    NODE_INSERTING = 2 // 节点正在插入
} node_state_t;

// 跳表节点结构
typedef struct skiplist_node {
    ref_count_t* ref_count;                     // 引用计数
    uint8_t* key;                              // 键
    uint32_t key_len;                          // 键长度
    void* value;                               // 值
    uint32_t value_len;                        // 值长度
    atomic_uint state;                         // 节点状态
    uint32_t level;                            // 节点层数
    _Atomic(struct skiplist_node*) next[];     // 后继节点数组
} skiplist_node_t;

// 跳表结构
typedef struct {
    skiplist_node_t* head;     // 头节点
    atomic_uint size;          // 元素个数
    uint32_t max_level;        // 最大层数
} atomic_skiplist_t;

// 创建跳表
atomic_skiplist_t* atomic_skiplist_create(void);

// 销毁跳表
void atomic_skiplist_destroy(atomic_skiplist_t* list);

// 插入键值对
int atomic_skiplist_put(atomic_skiplist_t* list, const uint8_t* key, size_t key_len, 
                       const uint8_t* value, size_t value_len);

// 删除键值对
int atomic_skiplist_delete(atomic_skiplist_t* list, const uint8_t* key, size_t key_len);

// 查找键值对
int atomic_skiplist_get(atomic_skiplist_t* list, const uint8_t* key, size_t key_len,
                       uint8_t* value, size_t* value_len);

// 获取元素个数
size_t atomic_skiplist_size(atomic_skiplist_t* list);

// 清空跳表
void atomic_skiplist_clear(atomic_skiplist_t* list);

// 遍历跳表
typedef bool (*skiplist_visitor_t)(const uint8_t* key, size_t key_len, 
                                 const uint8_t* value, size_t value_len, void* ctx);
void atomic_skiplist_foreach(atomic_skiplist_t* list, skiplist_visitor_t visitor, void* ctx);

#endif // PPDB_ATOMIC_SKIPLIST_H
