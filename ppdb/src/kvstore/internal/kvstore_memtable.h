#ifndef PPDB_KVSTORE_MEMTABLE_H
#define PPDB_KVSTORE_MEMTABLE_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"

// 内存表迭代器
typedef struct ppdb_memtable_iterator ppdb_memtable_iterator_t;

// 内存表配置
typedef struct ppdb_memtable_config {
    bool use_lockfree;      // 是否使用无锁模式
    size_t shard_count;     // 分片数量
    size_t size_limit;      // 大小限制
    uint32_t shard_bits;    // 分片位数
    size_t shard_size;      // 每个分片的大小
} ppdb_memtable_config_t;

// 内存表
typedef struct ppdb_memtable {
    ppdb_memtable_config_t config;  // 配置信息
    void* skiplist;                // 跳表数据结构
    ppdb_sync_t sync;              // 同步原语
    ppdb_metrics_t metrics;        // 性能指标
    bool is_immutable;             // 是否不可变
    size_t size;                   // 当前大小
    atomic_size_t used;           // 已使用大小
    void** shards;                // 分片数组
    ppdb_sync_t* shard_syncs;     // 分片同步原语数组
    atomic_size_t total_size;     // 总大小
} ppdb_memtable_t;

// 创建内存表
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table);
ppdb_error_t ppdb_memtable_create_sharded(size_t size_limit, ppdb_memtable_t** table);
ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** table);

// 基本操作
ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table, 
                              const void* key, size_t key_len,
                              const void* value, size_t value_len);

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              void** value, size_t* value_len);

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const void* key, size_t key_len);

// 状态查询
size_t ppdb_memtable_size(ppdb_memtable_t* table);
size_t ppdb_memtable_max_size(ppdb_memtable_t* table);
bool ppdb_memtable_is_immutable(ppdb_memtable_t* table);
void ppdb_memtable_set_immutable(ppdb_memtable_t* table);

// 迭代器操作
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* table, ppdb_memtable_iterator_t** iter);
ppdb_error_t ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter, 
                                        void** key, size_t* key_len,
                                        void** value, size_t* value_len);
void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter);

// 无锁版本的操作
ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table, 
                                       const void* key, size_t key_len,
                                       const void* value, size_t value_len);

ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table,
                                       const void* key, size_t key_len,
                                       void** value, size_t* value_len);

ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table,
                                          const void* key, size_t key_len);

// 生命周期管理
void ppdb_memtable_destroy(ppdb_memtable_t* table);
void ppdb_memtable_close(ppdb_memtable_t* table);
void ppdb_memtable_destroy_lockfree(ppdb_memtable_t* table);
void ppdb_memtable_close_lockfree(ppdb_memtable_t* table);

// 基础版本函数
ppdb_error_t ppdb_memtable_put_basic(ppdb_memtable_t* table,
                                    const void* key, size_t key_size,
                                    const void* value, size_t value_size);

// 无锁版本函数
ppdb_error_t ppdb_memtable_put_lockfree_basic(ppdb_memtable_t* table,
                                             const void* key, size_t key_size,
                                             const void* value, size_t value_size);

#endif // PPDB_KVSTORE_MEMTABLE_H 