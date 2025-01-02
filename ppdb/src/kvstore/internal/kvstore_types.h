#ifndef PPDB_KVSTORE_TYPES_H
#define PPDB_KVSTORE_TYPES_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/sync.h"

// 基础内存表结构
typedef struct ppdb_memtable_basic {
    ppdb_skiplist_t* skiplist;  // 跳表
    ppdb_sync_t sync;           // 同步原语
    size_t used;               // 已使用大小
    size_t size;               // 总大小限制
} ppdb_memtable_basic_t;

// 内存表分片结构
typedef struct ppdb_memtable_shard {
    ppdb_skiplist_t* skiplist;  // 分片跳表
    ppdb_sync_t sync;           // 分片同步原语
    atomic_size_t size;         // 分片大小
} ppdb_memtable_shard_t;

// 内存表结构
struct ppdb_memtable {
    ppdb_memtable_type_t type;  // 内存表类型
    size_t size_limit;          // 大小限制
    atomic_size_t current_size; // 当前大小
    size_t shard_count;         // 分片数量
    bool is_immutable;          // 是否不可变
    ppdb_metrics_t metrics;     // 性能指标
    union {
        ppdb_memtable_basic_t* basic;   // 基础内存表
        ppdb_memtable_shard_t* shards;  // 分片内存表
    };
};

// 内存表迭代器结构
struct ppdb_memtable_iterator {
    ppdb_memtable_t* table;           // 内存表指针
    ppdb_skiplist_iterator_t* it;     // 跳表迭代器
    bool valid;                       // 是否有效
    ppdb_kv_pair_t current_pair;      // 当前键值对
};

#endif // PPDB_KVSTORE_TYPES_H