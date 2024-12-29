#ifndef PPDB_WAL_H
#define PPDB_WAL_H

#include "ppdb/common/sync.h"

// WAL记录类型
typedef enum {
    WAL_RECORD_PUT,
    WAL_RECORD_DELETE
} ppdb_wal_record_type_t;

// WAL配置
typedef struct ppdb_wal_config {
    ppdb_sync_config_t sync_config;    // 同步配置
    size_t buffer_size;               // 写缓冲大小
    bool enable_group_commit;         // 启用组提交
    uint32_t group_commit_interval;   // 组提交间隔(ms)
    bool enable_async_flush;          // 启用异步刷盘
    bool enable_checksum;             // 启用校验和
} ppdb_wal_config_t;

// WAL结构
typedef struct ppdb_wal ppdb_wal_t;

// WAL恢复迭代器
typedef struct ppdb_wal_recovery_iter ppdb_wal_recovery_iter_t;

// 创建WAL
ppdb_wal_t* ppdb_wal_create(const char* filename, const ppdb_wal_config_t* config);

// 销毁WAL
void ppdb_wal_destroy(ppdb_wal_t* wal);

// 追加记录
int ppdb_wal_append(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                    const void* key, size_t key_len,
                    const void* value, size_t value_len,
                    uint64_t sequence);

// 同步到磁盘
int ppdb_wal_sync(ppdb_wal_t* wal);

// 创建恢复迭代器
ppdb_wal_recovery_iter_t* ppdb_wal_recovery_iter_create(ppdb_wal_t* wal);

// 销毁恢复迭代器
void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter);

// 恢复迭代器是否有效
bool ppdb_wal_recovery_iter_valid(ppdb_wal_recovery_iter_t* iter);

// 获取下一条记录
int ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                               ppdb_wal_record_type_t* type,
                               void** key, size_t* key_len,
                               void** value, size_t* value_len,
                               uint64_t* sequence);

#endif // PPDB_WAL_H
