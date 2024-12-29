#ifndef PPDB_MEMTABLE_H
#define PPDB_MEMTABLE_H

#include "ppdb/error.h"

// 前向声明
typedef struct ppdb_memtable_t ppdb_memtable_t;
typedef struct ppdb_memtable_iterator_t ppdb_memtable_iterator_t;

// 有锁版本
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table);
void ppdb_memtable_close(ppdb_memtable_t* table);
void ppdb_memtable_destroy(ppdb_memtable_t* table);
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table, const uint8_t* key, size_t key_len, const uint8_t* value, size_t value_len);
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table, const uint8_t* key, size_t key_len, uint8_t** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table, const uint8_t* key, size_t key_len);
size_t ppdb_memtable_size(ppdb_memtable_t* table);
size_t ppdb_memtable_max_size(ppdb_memtable_t* table);

// 无锁版本
ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** table);
void ppdb_memtable_close_lockfree(ppdb_memtable_t* table);
void ppdb_memtable_destroy_lockfree(ppdb_memtable_t* table);
ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table, const uint8_t* key, size_t key_len, const uint8_t* value, size_t value_len);
ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table, const uint8_t* key, size_t key_len, uint8_t** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table, const uint8_t* key, size_t key_len);
size_t ppdb_memtable_size_lockfree(ppdb_memtable_t* table);
size_t ppdb_memtable_max_size_lockfree(ppdb_memtable_t* table);

// 迭代器相关函数
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* table, ppdb_memtable_iterator_t** iter);
void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter);
bool ppdb_memtable_iterator_valid(ppdb_memtable_iterator_t* iter);
void ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter);
ppdb_error_t ppdb_memtable_iterator_get(ppdb_memtable_iterator_t* iter,
                                       const uint8_t** key, size_t* key_len,
                                       const uint8_t** value, size_t* value_len);

#endif // PPDB_MEMTABLE_H 