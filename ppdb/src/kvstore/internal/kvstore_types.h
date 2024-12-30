#ifndef PPDB_KVSTORE_TYPES_H
#define PPDB_KVSTORE_TYPES_H

#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/skiplist.h"

// 内存表类型枚举
typedef enum ppdb_memtable_type {
    PPDB_MEMTABLE_BASIC = 0,    // 基础版本
    PPDB_MEMTABLE_SHARDED,      // 分片版本
    PPDB_MEMTABLE_LOCKFREE      // 无锁版本
} ppdb_memtable_type_t;

// 基础内存表结构
typedef struct ppdb_memtable_basic {
    ppdb_skiplist_t* skiplist;  // 跳表
    ppdb_sync_t sync;           // 同步原语
    size_t size;                // 大小限制
    atomic_size_t used;         // 已使用大小
} ppdb_memtable_basic_t;

// 内存表分片结构
typedef struct ppdb_memtable_shard {
    ppdb_skiplist_t* skiplist;  // 分片的跳表
    ppdb_sync_t sync;           // 分片的同步原语
    atomic_size_t size;         // 分片当前大小
} ppdb_memtable_shard_t;

// 内存表
typedef struct ppdb_memtable {
    ppdb_memtable_type_t type;    // 内存表类型
    size_t size_limit;            // 大小限制
    atomic_size_t current_size;   // 当前大小
    size_t shard_count;          // 分片数量
    union {
        ppdb_memtable_basic_t* basic;   // 基础版本
        ppdb_memtable_shard_t* shards;  // 分片版本
    };
    ppdb_metrics_t metrics;       // 性能指标
    bool is_immutable;            // 是否不可变
} ppdb_memtable_t;

// 迭代器结构
typedef struct ppdb_iterator {
    void* internal;
    bool (*next)(struct ppdb_iterator*);
    bool (*valid)(const struct ppdb_iterator*);
    ppdb_error_t (*get)(struct ppdb_iterator*, ppdb_kv_pair_t*);
} ppdb_iterator_t;

#endif // PPDB_KVSTORE_TYPES_H 