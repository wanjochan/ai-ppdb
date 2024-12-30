#ifndef PPDB_KVSTORE_INTERNAL_H
#define PPDB_KVSTORE_INTERNAL_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore_memtable.h"
#include "kvstore_wal.h"
#include "kvstore_monitor.h"
#include "sync.h"

// KVStore内部结构
struct ppdb_kvstore {
    ppdb_sync_t sync;              // 同步原语（放在前面以优化对齐）
    ppdb_memtable_t* table;        // 当前内存表
    ppdb_wal_t* wal;               // WAL日志
    ppdb_monitor_t* monitor;       // 监控器
    char db_path[256];             // 数据库路径
    bool using_sharded;            // 是否使用分片
    bool adaptive_enabled;         // 是否启用自适应
    bool is_locked;               // 是否已锁定
};

// 内部函数声明
static ppdb_error_t create_memtable(size_t size, ppdb_memtable_t** table, bool use_sharding);
static ppdb_error_t create_wal(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
static ppdb_error_t handle_memtable_full(ppdb_kvstore_t* store,
                                        const void* key, size_t key_len,
                                        const void* value, size_t value_len);
static void cleanup_store(ppdb_kvstore_t* store);
static ppdb_error_t check_and_switch_memtable(ppdb_kvstore_t* store);

#endif // PPDB_KVSTORE_INTERNAL_H 