#ifndef PPDB_SKIPLIST_H
#define PPDB_SKIPLIST_H

#include <cosmopolitan.h>
#include "ppdb/memtable.h"

// 最大层数
#define PPDB_SKIPLIST_MAX_LEVEL 32

// 错误码
#define PPDB_ERR_INVALID_ARGUMENT PPDB_ERR_INVALID_ARG
#define PPDB_ERR_NOT_FOUND PPDB_ERR_KEY_NOT_FOUND

// 跳表节点结构
typedef struct ppdb_skiplist_node_t {
    uint8_t* key;
    size_t key_len;
    uint8_t* value;
    size_t value_len;
    int level;
    struct ppdb_skiplist_node_t* next[];  // 柔性数组
} ppdb_skiplist_node_t;

// 跳表结构
typedef struct ppdb_skiplist_t {
    ppdb_skiplist_node_t* head;    // 头节点
    int level;                     // 当前最大层数
    size_t size;                   // 节点数量
    pthread_mutex_t mutex;         // 并发控制
} ppdb_skiplist_t;

// 迭代器结构
typedef struct ppdb_skiplist_iterator_t {
    ppdb_skiplist_t* list;
    ppdb_skiplist_node_t* current;
} ppdb_skiplist_iterator_t;

// 创建跳表
ppdb_error_t ppdb_skiplist_create(ppdb_skiplist_t** list);

// 销毁跳表
void ppdb_skiplist_destroy(ppdb_skiplist_t* list);

// 插入/更新键值对
ppdb_error_t ppdb_skiplist_put(ppdb_skiplist_t* list,
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len);

// 获取键对应的值
ppdb_error_t ppdb_skiplist_get(ppdb_skiplist_t* list,
                              const uint8_t* key, size_t key_len,
                              uint8_t* value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_skiplist_delete(ppdb_skiplist_t* list,
                                 const uint8_t* key, size_t key_len);

// 获取当前大小
size_t ppdb_skiplist_size(ppdb_skiplist_t* list);

// 迭代器操作
ppdb_error_t ppdb_skiplist_iterator_create(ppdb_skiplist_t* list, ppdb_skiplist_iterator_t** iter);
void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* iter);
bool ppdb_skiplist_iterator_valid(ppdb_skiplist_iterator_t* iter);
void ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* iter);
void ppdb_skiplist_iterator_seek(ppdb_skiplist_iterator_t* iter, const uint8_t* key, size_t key_len);
ppdb_error_t ppdb_skiplist_iterator_key(ppdb_skiplist_iterator_t* iter, const uint8_t** key, size_t* key_len);
ppdb_error_t ppdb_skiplist_iterator_value(ppdb_skiplist_iterator_t* iter, const uint8_t** value, size_t* value_len);

#endif // PPDB_SKIPLIST_H 