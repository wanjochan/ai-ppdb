#ifndef PPDB_SKIPLIST_H_
#define PPDB_SKIPLIST_H_

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/sync.h"

// 比较函数类型
typedef int (*ppdb_compare_func_t)(const void* key1, size_t key1_len,
                                const void* key2, size_t key2_len);

// 跳表结构
typedef struct ppdb_skiplist ppdb_skiplist_t;

// 迭代器结构
typedef struct ppdb_skiplist_iterator ppdb_skiplist_iterator_t;

// 跳表操作
ppdb_error_t ppdb_skiplist_create(ppdb_skiplist_t** list, int max_level,
                                ppdb_compare_func_t compare,
                                const ppdb_sync_config_t* sync_config);
void ppdb_skiplist_destroy(ppdb_skiplist_t* list);
ppdb_error_t ppdb_skiplist_put(ppdb_skiplist_t* list, const void* key, size_t key_len,
                             const void* value, size_t value_len);
ppdb_error_t ppdb_skiplist_get(ppdb_skiplist_t* list, const void* key, size_t key_len,
                             void** value, size_t* value_len);
ppdb_error_t ppdb_skiplist_delete(ppdb_skiplist_t* list, const void* key, size_t key_len);
void ppdb_skiplist_clear(ppdb_skiplist_t* list);

// 统计信息
size_t ppdb_skiplist_size(const ppdb_skiplist_t* list);
size_t ppdb_skiplist_memory_usage(const ppdb_skiplist_t* list);
bool ppdb_skiplist_empty(const ppdb_skiplist_t* list);

// 迭代器操作
ppdb_error_t ppdb_skiplist_iterator_create(ppdb_skiplist_t* list,
                                        ppdb_skiplist_iterator_t** iter,
                                        const ppdb_sync_config_t* sync_config);
void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* iter);
ppdb_error_t ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* iter);
ppdb_error_t ppdb_skiplist_iterator_get(ppdb_skiplist_iterator_t* iter,
                                     ppdb_kv_pair_t* pair);
bool ppdb_skiplist_iterator_valid(const ppdb_skiplist_iterator_t* iter);

// 默认比较函数
int ppdb_skiplist_default_compare(const void* key1, size_t key1_len,
                               const void* key2, size_t key2_len);

#endif // PPDB_SKIPLIST_H_ 