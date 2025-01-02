#ifndef PPDB_STORAGE_H_
#define PPDB_STORAGE_H_

#include "cosmopolitan.h"
#include "ppdb/base.h"

#ifdef __cplusplus
extern "C" {
#endif

// 存储配置
typedef struct {
    size_t initial_size;
    size_t max_size;
    uint32_t flags;
    void* user_data;
} ppdb_storage_config_t;

// 存储统计
typedef struct {
    ppdb_metrics_t base_metrics;  // 基础统计信息
    atomic_size_t bytes_read;     // 读取字节数
    atomic_size_t bytes_written;  // 写入字节数
    atomic_size_t cache_hits;     // 缓存命中次数
    atomic_size_t cache_misses;   // 缓存未命中次数
} ppdb_storage_stats_t;

// Skiplist特化
ppdb_error_t ppdb_skiplist_create(ppdb_base_t* base, const ppdb_storage_config_t* config);
ppdb_error_t ppdb_skiplist_destroy(ppdb_base_t* base);

// Memtable特化
ppdb_error_t ppdb_memtable_create(ppdb_base_t* base, const ppdb_storage_config_t* config);
ppdb_error_t ppdb_memtable_destroy(ppdb_base_t* base);

// Sharded特化
ppdb_error_t ppdb_sharded_create(ppdb_base_t* base, const ppdb_storage_config_t* config);
ppdb_error_t ppdb_sharded_destroy(ppdb_base_t* base);

// KVStore特化
ppdb_error_t ppdb_kvstore_create(ppdb_base_t* base, const ppdb_storage_config_t* config);
ppdb_error_t ppdb_kvstore_destroy(ppdb_base_t* base);

// 通用操作
ppdb_error_t ppdb_storage_sync(ppdb_base_t* base);
ppdb_error_t ppdb_storage_flush(ppdb_base_t* base);
ppdb_error_t ppdb_storage_compact(ppdb_base_t* base);
ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_storage_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif // PPDB_STORAGE_H_
