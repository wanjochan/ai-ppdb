#ifndef PPDB_KVSTORE_H
#define PPDB_KVSTORE_H

#include "ppdb_error.h"
#include "ppdb_types.h"
#include "base.h"
#include "storage.h"
#include <cosmopolitan.h>

// KV存储层接口
typedef struct {
    ppdb_error_t (*begin_tx)(void* impl);
    ppdb_error_t (*commit_tx)(void* impl);
    ppdb_error_t (*snapshot)(void* impl, void** snap);
    ppdb_error_t (*compact)(void* impl);
    ppdb_error_t (*get_stats)(void* impl, ppdb_storage_stats_t* stats);
} ppdb_kvstore_ops_t;

// KV存储层实现
typedef struct {
    ppdb_base_t base;          // 24字节
    ppdb_kvstore_ops_t* ops;   // 8字节
    ppdb_container_t* active;  // 8字节
    ppdb_container_t* imm;     // 8字节
    ppdb_storage_t* wal;       // 8字节
    ppdb_storage_t** sst;      // 8字节
    ppdb_metrics_t metrics;    // 统计信息
} ppdb_kvstore_t;             // 总大小：64字节 + metrics

// KV存储配置
typedef struct {
    size_t memtable_size;     // 内存表大小限制
    size_t l0_size;           // L0层大小限制
    size_t block_size;        // 块大小
    bool enable_compression;   // 是否启用压缩
    bool enable_bloom_filter; // 是否启用布隆过滤器
    ppdb_storage_config_t storage; // 存储配置
} ppdb_kvstore_config_t;

// 创建和销毁
ppdb_error_t ppdb_kvstore_create(ppdb_base_t* base, const ppdb_kvstore_config_t* config);
ppdb_error_t ppdb_kvstore_destroy(ppdb_base_t* base);

// 事务操作
ppdb_error_t ppdb_kvstore_begin_tx(ppdb_base_t* base);
ppdb_error_t ppdb_kvstore_commit_tx(ppdb_base_t* base);
ppdb_error_t ppdb_kvstore_rollback_tx(ppdb_base_t* base);

// 快照操作
ppdb_error_t ppdb_kvstore_create_snapshot(ppdb_base_t* base, void** snapshot);
ppdb_error_t ppdb_kvstore_release_snapshot(ppdb_base_t* base, void* snapshot);

// 维护操作
ppdb_error_t ppdb_kvstore_compact(ppdb_base_t* base);
ppdb_error_t ppdb_kvstore_flush(ppdb_base_t* base);
ppdb_error_t ppdb_kvstore_get_stats(ppdb_base_t* base, ppdb_storage_stats_t* stats);

#endif // PPDB_KVSTORE_H
