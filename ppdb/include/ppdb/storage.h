#ifndef PPDB_STORAGE_H
#define PPDB_STORAGE_H

#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// 跳表操作
ppdb_error_t skiplist_init(ppdb_base_t* base, const ppdb_storage_config_t* config);
ppdb_error_t skiplist_destroy(ppdb_base_t* base);
ppdb_error_t skiplist_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t skiplist_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t skiplist_remove(ppdb_base_t* base, const ppdb_key_t* key);

// 内存表操作
ppdb_error_t memtable_init(ppdb_base_t* base, const ppdb_storage_config_t* config);
ppdb_error_t memtable_destroy(ppdb_base_t* base);
ppdb_error_t memtable_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t memtable_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t memtable_remove(ppdb_base_t* base, const ppdb_key_t* key);

// KV存储操作
ppdb_error_t kvstore_init(ppdb_base_t* base, const ppdb_storage_config_t* config, ppdb_kvstore_t** store);
ppdb_error_t kvstore_destroy(ppdb_kvstore_t* store);
ppdb_error_t kvstore_get(ppdb_kvstore_t* store, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t kvstore_put(ppdb_kvstore_t* store, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t kvstore_remove(ppdb_kvstore_t* store, const ppdb_key_t* key);

// 分片存储操作
ppdb_error_t sharded_init(ppdb_base_t* base, const ppdb_storage_config_t* config);
ppdb_error_t sharded_destroy(ppdb_base_t* base);
ppdb_error_t sharded_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t sharded_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t sharded_remove(ppdb_base_t* base, const ppdb_key_t* key);

// 存储统计
ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_storage_stats_t* stats);

// 存储同步
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base);
ppdb_error_t ppdb_storage_flush(ppdb_base_t* base);
ppdb_error_t ppdb_storage_compact(ppdb_base_t* base);

#ifdef __cplusplus
}
#endif

#endif // PPDB_STORAGE_H
