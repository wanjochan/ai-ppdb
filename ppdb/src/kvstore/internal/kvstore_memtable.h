#ifndef PPDB_KVSTORE_MEMTABLE_H
#define PPDB_KVSTORE_MEMTABLE_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/skiplist.h"

// 内存表配置
typedef struct ppdb_memtable_config {
    ppdb_memtable_type_t type;  // 内存表类型
    size_t size_limit;          // 大小限制
    size_t shard_count;         // 分片数量
    ppdb_sync_config_t sync;    // 同步配置
} ppdb_memtable_config_t;

// 函数声明
// 基础内存表操作
ppdb_error_t ppdb_memtable_create_basic(size_t size_limit, ppdb_memtable_t** table);
void ppdb_memtable_destroy_basic(ppdb_memtable_t* table);
ppdb_error_t ppdb_memtable_put_basic(ppdb_memtable_t* table, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_memtable_get_basic(ppdb_memtable_t* table, const void* key, size_t key_len, void** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete_basic(ppdb_memtable_t* table, const void* key, size_t key_len);
size_t ppdb_memtable_size_basic(ppdb_memtable_t* table);
size_t ppdb_memtable_max_size_basic(ppdb_memtable_t* table);
bool ppdb_memtable_is_immutable_basic(ppdb_memtable_t* table);
void ppdb_memtable_set_immutable_basic(ppdb_memtable_t* table);
const ppdb_metrics_t* ppdb_memtable_get_metrics_basic(ppdb_memtable_t* table);

// 分片内存表操作
ppdb_error_t ppdb_memtable_create_sharded_basic(size_t size_limit, ppdb_memtable_t** table);
void ppdb_memtable_destroy_sharded(ppdb_memtable_t* table);
ppdb_error_t ppdb_memtable_put_sharded_basic(ppdb_memtable_t* table, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_memtable_get_sharded_basic(ppdb_memtable_t* table, const void* key, size_t key_len, void** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete_sharded_basic(ppdb_memtable_t* table, const void* key, size_t key_len);

// 无锁内存表操作
ppdb_error_t ppdb_memtable_put_lockfree_basic(ppdb_memtable_t* table, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_memtable_get_lockfree_basic(ppdb_memtable_t* table, const void* key, size_t key_len, void** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete_lockfree_basic(ppdb_memtable_t* table, const void* key, size_t key_len);
ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table, const void* key, size_t key_len, void** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table, const void* key, size_t key_len);

// 内存表迭代器基本操作
ppdb_error_t ppdb_memtable_iterator_create_basic(ppdb_memtable_t* table,
                                                ppdb_memtable_iterator_t** iter);
ppdb_error_t ppdb_memtable_iterator_next_basic(ppdb_memtable_iterator_t* iter,
                                              ppdb_kv_pair_t** pair);
ppdb_error_t ppdb_memtable_iterator_get_basic(ppdb_memtable_iterator_t* iter,
                                             ppdb_kv_pair_t* pair);
void ppdb_memtable_iterator_destroy_basic(ppdb_memtable_iterator_t* iter);

// 工厂函数
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** memtable);
ppdb_error_t ppdb_memtable_create_sharded(size_t size_limit, ppdb_memtable_t** memtable);
ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** memtable);

// 通用操作
void ppdb_memtable_destroy(ppdb_memtable_t* memtable);
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* memtable, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* memtable, const void* key, size_t key_len, void** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* memtable, const void* key, size_t key_len);
void ppdb_memtable_clear(ppdb_memtable_t* memtable);
size_t ppdb_memtable_size(ppdb_memtable_t* memtable);
bool ppdb_memtable_empty(ppdb_memtable_t* memtable);
bool ppdb_memtable_full(ppdb_memtable_t* memtable);
size_t ppdb_memtable_max_size(ppdb_memtable_t* memtable);

#endif // PPDB_KVSTORE_MEMTABLE_H