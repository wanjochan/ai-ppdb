#ifndef PPDB_MEMTABLE_H
#define PPDB_MEMTABLE_H

#include <cosmopolitan.h>
#include "ppdb/error.h"

// MemTable句柄
typedef struct ppdb_memtable_t ppdb_memtable_t;

// 创建MemTable
ppdb_error_t ppdb_memtable_create(size_t max_size, ppdb_memtable_t** table);

// 销毁MemTable
void ppdb_memtable_destroy(ppdb_memtable_t* table);

// 插入/更新键值对
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              const uint8_t* value, size_t value_len);

// 获取键对应的值
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const uint8_t* key, size_t key_len,
                              uint8_t* value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const uint8_t* key, size_t key_len);

// 获取当前大小
size_t ppdb_memtable_size(ppdb_memtable_t* table);

// 获取最大大小
size_t ppdb_memtable_max_size(ppdb_memtable_t* table);

#endif // PPDB_MEMTABLE_H 