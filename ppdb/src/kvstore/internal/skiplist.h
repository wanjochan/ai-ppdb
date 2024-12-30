#ifndef PPDB_SKIPLIST_H
#define PPDB_SKIPLIST_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 最大层数
#define PPDB_SKIPLIST_MAX_LEVEL 32

// 跳表节点
typedef struct ppdb_skiplist_node {
    void* key;                          // 键
    size_t key_len;                     // 键长度
    void* value;                        // 值
    size_t value_len;                   // 值长度
    struct ppdb_skiplist_node** next;   // 后继节点数组
    int level;                          // 当前节点层数
} ppdb_skiplist_node_t;

// 跳表
typedef struct ppdb_skiplist {
    ppdb_skiplist_node_t* head;         // 头节点
    int level;                          // 当前最大层数
    size_t size;                        // 节点数量
    size_t memory_usage;                // 内存使用量
    int (*compare)(const void* key1, size_t key1_len,
                  const void* key2, size_t key2_len); // 比较函数
} ppdb_skiplist_t;

// 跳表迭代器
typedef struct ppdb_skiplist_iterator {
    ppdb_skiplist_t* list;              // 跳表指针
    ppdb_skiplist_node_t* current;      // 当前节点
} ppdb_skiplist_iterator_t;

// 创建跳表
ppdb_error_t ppdb_skiplist_create(ppdb_skiplist_t** list,
                                 int (*compare)(const void* key1, size_t key1_len,
                                              const void* key2, size_t key2_len));

// 销毁跳表
void ppdb_skiplist_destroy(ppdb_skiplist_t* list);

// 插入键值对
ppdb_error_t ppdb_skiplist_put(ppdb_skiplist_t* list,
                              const void* key, size_t key_len,
                              const void* value, size_t value_len);

// 获取键值对
ppdb_error_t ppdb_skiplist_get(ppdb_skiplist_t* list,
                              const void* key, size_t key_len,
                              void* value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_skiplist_delete(ppdb_skiplist_t* list,
                                 const void* key, size_t key_len);

// 创建迭代器
ppdb_error_t ppdb_skiplist_iterator_create(ppdb_skiplist_t* list,
                                         ppdb_skiplist_iterator_t** iter);

// 迭代器获取下一个节点
ppdb_error_t ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* iter,
                                       void** key, size_t* key_len,
                                       void** value, size_t* value_len);

// 销毁迭代器
void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* iter);

// 获取跳表大小
size_t ppdb_skiplist_size(ppdb_skiplist_t* list);

// 获取跳表内存使用量
size_t ppdb_skiplist_memory_usage(ppdb_skiplist_t* list);

// 检查跳表是否为空
bool ppdb_skiplist_empty(ppdb_skiplist_t* list);

// 清空跳表
void ppdb_skiplist_clear(ppdb_skiplist_t* list);

#endif // PPDB_SKIPLIST_H 