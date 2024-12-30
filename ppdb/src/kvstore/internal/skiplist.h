#ifndef PPDB_KVSTORE_INTERNAL_SKIPLIST_H
#define PPDB_KVSTORE_INTERNAL_SKIPLIST_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 跳表结构
typedef struct ppdb_skiplist ppdb_skiplist_t;

// 跳表迭代器结构
typedef struct ppdb_skiplist_iterator ppdb_skiplist_iterator_t;

// 创建跳表
ppdb_error_t ppdb_skiplist_create(ppdb_skiplist_t** list);

// 销毁跳表
void ppdb_skiplist_destroy(ppdb_skiplist_t* list);

// 插入键值对
ppdb_error_t ppdb_skiplist_put(ppdb_skiplist_t* list,
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len);

// 获取键值对
ppdb_error_t ppdb_skiplist_get(ppdb_skiplist_t* list,
                              const uint8_t* key, size_t key_len,
                              uint8_t** value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_skiplist_delete(ppdb_skiplist_t* list,
                                 const uint8_t* key, size_t key_len);

// 获取大小
size_t ppdb_skiplist_size(ppdb_skiplist_t* list);

// 创建迭代器
ppdb_error_t ppdb_skiplist_iterator_create(ppdb_skiplist_t* list,
                                         ppdb_skiplist_iterator_t** iter);

// 迭代器移动到下一个元素
ppdb_error_t ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* iter,
                                        uint8_t** key, size_t* key_len,
                                        uint8_t** value, size_t* value_len);

// 销毁迭代器
void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* iter);

#endif // PPDB_KVSTORE_INTERNAL_SKIPLIST_H 