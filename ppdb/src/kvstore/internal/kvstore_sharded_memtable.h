#ifndef PPDB_KVSTORE_SHARDED_MEMTABLE_H
#define PPDB_KVSTORE_SHARDED_MEMTABLE_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "kvstore_memtable.h"

// 分片内存表类型
typedef struct ppdb_sharded_memtable ppdb_sharded_memtable_t;

// 创建分片内存表
ppdb_error_t ppdb_sharded_memtable_create(ppdb_sharded_memtable_t** table, size_t shard_count);

// 销毁分片内存表
void ppdb_sharded_memtable_destroy(ppdb_sharded_memtable_t* table);

// 基本操作
ppdb_error_t ppdb_sharded_memtable_put(ppdb_sharded_memtable_t* table, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_sharded_memtable_get(ppdb_sharded_memtable_t* table, const void* key, size_t key_len, void* value, size_t* value_len);
ppdb_error_t ppdb_sharded_memtable_delete(ppdb_sharded_memtable_t* table, const void* key, size_t key_len);

#endif // PPDB_KVSTORE_SHARDED_MEMTABLE_H 