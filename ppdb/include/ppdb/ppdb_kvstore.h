#ifndef PPDB_PUBLIC_KVSTORE_H
#define PPDB_PUBLIC_KVSTORE_H

#include <cosmopolitan.h>
#include "ppdb_error.h"

// KVStore配置
typedef struct ppdb_kvstore_config {
    char data_dir[256];         // 数据目录
    size_t memtable_size;       // 内存表大小限制
    bool use_sharding;          // 是否使用分片
    bool adaptive_sharding;     // 是否启用自适应分片
    bool enable_compression;    // 是否启用压缩
    bool enable_monitoring;     // 是否启用监控
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

#endif // PPDB_PUBLIC_KVSTORE_H 