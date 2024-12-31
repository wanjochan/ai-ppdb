#ifndef PPDB_KVSTORE_WAL_H
#define PPDB_KVSTORE_WAL_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/kvstore_wal_types.h"

// WAL配置
typedef struct {
    const char* dir;               // WAL目录路径
    ppdb_sync_mode_t sync_mode;   // 同步模式
} ppdb_wal_config_t;

// 基础WAL操作
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal);

// 工厂函数
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal);
size_t ppdb_wal_size(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal);

#endif // PPDB_KVSTORE_WAL_H