#ifndef PPDB_KVSTORE_INTERNAL_H
#define PPDB_KVSTORE_INTERNAL_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore_memtable.h"
#include "kvstore_wal.h"
#include "kvstore_monitor.h"

// KVStore内部结构
struct ppdb_kvstore {
    char db_path[256];              // 数据库路径
    ppdb_memtable_t* table;         // 当前内存表
    ppdb_wal_t* wal;                // WAL日志
    ppdb_monitor_t* monitor;        // 监控器
    bool using_sharded;             // 是否使用分片
    bool adaptive_enabled;          // 是否启用自适应
    mutex_t mutex;                  // 互斥锁
    bool is_locked;                 // 是否已锁定
};

#endif // PPDB_KVSTORE_INTERNAL_H 