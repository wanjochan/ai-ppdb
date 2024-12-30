#ifndef PPDB_SKIPLIST_H
#define PPDB_SKIPLIST_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/sync.h"

// 跳表操作函数声明

// 创建跳表
ppdb_error_t ppdb_skiplist_create(ppdb_skiplist_t** list, 
                                int max_level,
                                ppdb_compare_func_t compare_func,
                                const ppdb_sync_config_t* sync_config);

// 销毁跳表
void ppdb_skiplist_destroy(ppdb_skiplist_t* list);

// 插入键值对
ppdb_error_t ppdb_skiplist_put(ppdb_skiplist_t* list,
                              const void* key, size_t key_len,
                              const void* value, size_t value_len);

// 获取键值对
ppdb_error_t ppdb_skiplist_get(ppdb_skiplist_t* list,
                              const void* key, size_t key_len,
                              void** value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_skiplist_delete(ppdb_skiplist_t* list,
                                 const void* key, size_t key_len);

// 清空跳表
void ppdb_skiplist_clear(ppdb_skiplist_t* list);

// 获取跳表大小
size_t ppdb_skiplist_size(ppdb_skiplist_t* list);

// 获取跳表内存使用量
size_t ppdb_skiplist_memory_usage(ppdb_skiplist_t* list);

// 检查跳表是否为空
bool ppdb_skiplist_empty(ppdb_skiplist_t* list);

// 默认比较函数
int ppdb_skiplist_default_compare(const void* key1, size_t key1_len,
                                const void* key2, size_t key2_len);

// 迭代器相关操作
// 创建迭代器
ppdb_error_t ppdb_skiplist_iterator_create(ppdb_skiplist_t* list,
                                         ppdb_skiplist_iterator_t** iter,
                                         const ppdb_sync_config_t* sync_config);

// 销毁迭代器
void ppdb_skiplist_iterator_destroy(ppdb_skiplist_iterator_t* iter);

// 迭代器移动到下一个元素
ppdb_error_t ppdb_skiplist_iterator_next(ppdb_skiplist_iterator_t* iter,
                                       void** key, size_t* key_len,
                                       void** value, size_t* value_len);

// 检查迭代器是否有效
bool ppdb_skiplist_iterator_valid(ppdb_skiplist_iterator_t* iter);

// 获取迭代器当前键值对
ppdb_error_t ppdb_skiplist_iterator_get(ppdb_skiplist_iterator_t* iter,
                                      ppdb_kv_pair_t* pair);

// 无锁操作函数
ppdb_error_t ppdb_skiplist_put_lockfree(ppdb_skiplist_t* list,
                                      const void* key, size_t key_len,
                                      const void* value, size_t value_len);

ppdb_error_t ppdb_skiplist_get_lockfree(ppdb_skiplist_t* list,
                                      const void* key, size_t key_len,
                                      void** value, size_t* value_len);

ppdb_error_t ppdb_skiplist_delete_lockfree(ppdb_skiplist_t* list,
                                         const void* key, size_t key_len);

#endif // PPDB_SKIPLIST_H 