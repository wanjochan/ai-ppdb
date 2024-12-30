#ifndef PPDB_KVSTORE_WAL_H
#define PPDB_KVSTORE_WAL_H

#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"
#include "kvstore/internal/kvstore_memtable.h"

// WAL 魔数
#define WAL_MAGIC 0x4C415750  // "PWAL"

// WAL 文件头
typedef struct wal_header {
    uint32_t magic;      // 魔数
    uint32_t version;    // 版本号
    uint64_t sequence;   // 序列号
} wal_header_t;

// WAL 记录类型
typedef enum ppdb_wal_record_type {
    PPDB_WAL_RECORD_PUT,
    PPDB_WAL_RECORD_DELETE
} ppdb_wal_record_type_t;

// WAL 记录头
typedef struct wal_record_header {
    ppdb_wal_record_type_t type;
    size_t key_size;
    size_t value_size;
    uint32_t crc32;      // CRC32校验和
} wal_record_header_t;

// WAL 缓冲区
typedef struct wal_buffer {
    char* data;
    size_t size;
    size_t used;
    bool in_use;
    ppdb_sync_t sync;
} wal_buffer_t;

// WAL 结构
typedef struct ppdb_wal {
    char* filename;
    size_t file_size;
    ppdb_sync_t sync;
    bool sync_on_write;
    bool enable_compression;
    
    // 缓冲区管理
    wal_buffer_t* buffers;
    size_t buffer_count;
    size_t current_buffer;
    
    // 状态管理
    uint64_t next_sequence;
    bool closed;
    
    // 性能指标
    ppdb_metrics_t metrics;
} ppdb_wal_t;

// WAL 恢复迭代器
typedef struct ppdb_wal_recovery_iter {
    ppdb_wal_t* wal;
    off_t position;
    char* buffer;
    size_t buffer_size;
    ppdb_kv_pair_t current;
} ppdb_wal_recovery_iter_t;

// WAL 配置
typedef struct ppdb_wal_config {
    char* data_dir;              // 数据目录
    size_t buffer_size;          // 缓冲区大小
    size_t buffer_count;         // 缓冲区数量
    bool sync_on_write;          // 写入时同步
    bool enable_compression;      // 启用压缩
    ppdb_sync_config_t sync;     // 同步配置
} ppdb_wal_config_t;

// 函数声明
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                           const void* key, size_t key_size,
                           const void* value, size_t value_size);
ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* table);

// 无锁版本函数
void ppdb_wal_close_lockfree(ppdb_wal_t* wal);
void ppdb_wal_destroy_lockfree(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                                    const void* key, size_t key_size,
                                    const void* value, size_t value_size);
ppdb_error_t ppdb_wal_sync_lockfree(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_recover_lockfree(ppdb_wal_t* wal, ppdb_memtable_t* table);

// 恢复迭代器操作
ppdb_error_t ppdb_wal_recovery_iter_create(ppdb_wal_t* wal, ppdb_wal_recovery_iter_t** iter);
ppdb_error_t ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter, void** key, size_t* key_size, void** value, size_t* value_size);
void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter);

// WAL 状态操作
bool ppdb_wal_is_closed(ppdb_wal_t* wal);
size_t ppdb_wal_size(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal);

#endif // PPDB_KVSTORE_WAL_H 