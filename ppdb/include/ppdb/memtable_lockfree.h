#ifndef PPDB_MEMTABLE_LOCKFREE_H
#define PPDB_MEMTABLE_LOCKFREE_H

#include <cosmopolitan.h>
#include "ppdb/error.h"

// MemTable结构
typedef struct ppdb_memtable_t ppdb_memtable_t;

// 创建无锁MemTable
ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** table);

// 销毁无锁MemTable
void ppdb_memtable_destroy_lockfree(ppdb_memtable_t* table);

// 写入键值对
ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table,
                                       const uint8_t* key, size_t key_len,
                                       const uint8_t* value, size_t value_len);

// 读取键值对
ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table,
                                       const uint8_t* key, size_t key_len,
                                       uint8_t** value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table,
                                          const uint8_t* key, size_t key_len);

// 获取当前大小
size_t ppdb_memtable_size_lockfree(ppdb_memtable_t* table);

// 获取最大大小限制
size_t ppdb_memtable_max_size_lockfree(ppdb_memtable_t* table);

#endif // PPDB_MEMTABLE_LOCKFREE_H 