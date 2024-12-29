#ifndef PPDB_MEMTABLE_H
#define PPDB_MEMTABLE_H

#include "error.h"
#include "defs.h"

// 内存表迭代器
typedef struct ppdb_memtable_iterator ppdb_memtable_iterator_t;

// 内存表
typedef struct ppdb_memtable ppdb_memtable_t;

// 创建内存表
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table);
ppdb_error_t ppdb_memtable_create_sharded(size_t size_limit, ppdb_memtable_t** table);
ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** table);

// 基本操作
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table, 
                              const void* key, size_t key_len,
                              const void* value, size_t value_len);

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              void** value, size_t* value_len);

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const void* key, size_t key_len);

// 状态查询
size_t ppdb_memtable_size(ppdb_memtable_t* table);
size_t ppdb_memtable_max_size(ppdb_memtable_t* table);
bool ppdb_memtable_is_immutable(ppdb_memtable_t* table);
void ppdb_memtable_set_immutable(ppdb_memtable_t* table);

// 迭代器操作
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* table, ppdb_memtable_iterator_t** iter);
ppdb_error_t ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter, 
                                        void** key, size_t* key_len,
                                        void** value, size_t* value_len);
void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter);

// 无锁版本的操作
ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table, 
                                       const void* key, size_t key_len,
                                       const void* value, size_t value_len);

ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table,
                                       const void* key, size_t key_len,
                                       void** value, size_t* value_len);

ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table,
                                          const void* key, size_t key_len);

// 生命周期管理
void ppdb_memtable_destroy(ppdb_memtable_t* table);
void ppdb_memtable_close(ppdb_memtable_t* table);
void ppdb_memtable_destroy_lockfree(ppdb_memtable_t* table);
void ppdb_memtable_close_lockfree(ppdb_memtable_t* table);

#endif // PPDB_MEMTABLE_H 