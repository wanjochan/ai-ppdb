#ifndef PPDB_KVSTORE_H
#define PPDB_KVSTORE_H

#include "ppdb_error.h"
#include "ppdb_types.h"
#include "ppdb_wal.h"
#include <cosmopolitan.h>

// 前向声明
typedef struct ppdb_iterator ppdb_iterator_t;
typedef struct ppdb_wal ppdb_wal_t;

// KVStore配置
typedef struct ppdb_kvstore_config {
    char data_dir[PPDB_MAX_PATH_SIZE];  // 数据目录
    size_t memtable_size;               // 内存表大小限制
    bool use_sharding;                  // 是否使用分片
    bool adaptive_sharding;             // 是否启用自适应分片
    bool enable_compression;            // 是否启用压缩
    bool enable_monitoring;             // 是否启用监控
    ppdb_wal_config_t wal;              // WAL配置
} ppdb_kvstore_config_t;

// KVStore句柄
typedef struct ppdb_kvstore ppdb_kvstore_t;

// 创建KVStore
ppdb_error_t ppdb_kvstore_create(const ppdb_kvstore_config_t* config, ppdb_kvstore_t** store);

// 基本操作
ppdb_error_t ppdb_kvstore_put(ppdb_kvstore_t* store,
                             const void* key, size_t key_len,
                             const void* value, size_t value_len);

ppdb_error_t ppdb_kvstore_get(ppdb_kvstore_t* store,
                             const void* key, size_t key_len,
                             void** value, size_t* value_len);

ppdb_error_t ppdb_kvstore_delete(ppdb_kvstore_t* store,
                                const void* key, size_t key_len);

// 生命周期管理
void ppdb_kvstore_close(ppdb_kvstore_t* store);
void ppdb_kvstore_destroy(ppdb_kvstore_t* store);

// 迭代器操作
void ppdb_iterator_destroy(ppdb_iterator_t* iter);

// WAL操作
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_close(ppdb_wal_t* wal);
void ppdb_wal_destroy(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal);
size_t ppdb_wal_size(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal);

#endif // PPDB_KVSTORE_H 