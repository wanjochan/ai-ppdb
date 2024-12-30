#ifndef PPDB_KVSTORE_MEMTABLE_H
#define PPDB_KVSTORE_MEMTABLE_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/skiplist.h"

// 内存表类型
typedef enum ppdb_memtable_type {
    PPDB_MEMTABLE_BASIC = 0,  // 基本内存表
    PPDB_MEMTABLE_SHARDED,    // 分片内存表
    PPDB_MEMTABLE_LOCKFREE    // 无锁内存表
} ppdb_memtable_type_t;

// 内存表配置
typedef struct ppdb_memtable_config {
    ppdb_memtable_type_t type;  // 内存表类型
    size_t size_limit;          // 大小限制
    size_t shard_count;         // 分片数量
    ppdb_sync_config_t sync;    // 同步配置
} ppdb_memtable_config_t;

// 内存表分片
typedef struct ppdb_memtable_shard {
    ppdb_skiplist_t* skiplist;  // 跳表
    ppdb_sync_t sync;           // 同步原语
    atomic_size_t size;         // 当前大小
} ppdb_memtable_shard_t;

// 内存表
typedef struct ppdb_memtable {
    ppdb_memtable_type_t type;      // 内存表类型
    ppdb_memtable_config_t config;   // 配置
    ppdb_sync_t sync;                // 同步原语
    atomic_size_t current_size;      // 当前大小
    size_t size_limit;               // 大小限制
    size_t shard_count;              // 分片数量
    ppdb_memtable_shard_t* shards;   // 分片数组
} ppdb_memtable_t;

// 初始化内存表
ppdb_error_t ppdb_memtable_init(ppdb_memtable_t* memtable, const ppdb_memtable_config_t* config);

// 销毁内存表
void ppdb_memtable_destroy(ppdb_memtable_t* memtable);

// 插入键值对
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* memtable, const void* key, size_t key_len, const void* value, size_t value_len);

// 获取键值对
ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* memtable, const void* key, size_t key_len, void* value, size_t* value_len);

// 删除键值对
ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* memtable, const void* key, size_t key_len);

// 清空内存表
void ppdb_memtable_clear(ppdb_memtable_t* memtable);

// 获取内存表大小
size_t ppdb_memtable_size(ppdb_memtable_t* memtable);

// 检查内存表是否为空
bool ppdb_memtable_empty(ppdb_memtable_t* memtable);

// 检查内存表是否已满
bool ppdb_memtable_full(ppdb_memtable_t* memtable);

#endif // PPDB_KVSTORE_MEMTABLE_H 