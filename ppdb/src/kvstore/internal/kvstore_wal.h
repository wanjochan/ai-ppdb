#ifndef PPDB_KVSTORE_WAL_H
#define PPDB_KVSTORE_WAL_H

#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore_types.h"
#include "sync.h"
#include "metrics.h"
#include "kvstore_memtable.h"

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

// 错误码定义
#define PPDB_ERROR_INVALID_ARGUMENT PPDB_ERR_INVALID_ARG
#define PPDB_ERROR_ITERATOR_END PPDB_ERR_NOT_FOUND
#define PPDB_ERROR_OK PPDB_OK
#define PPDB_ERROR_BUFFER_TOO_SMALL PPDB_ERR_NO_MEMORY
#define PPDB_ERROR_CLOSED PPDB_ERR_CLOSED
#define PPDB_ERROR_CHECKSUM PPDB_ERR_CHECKSUM

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

#endif // PPDB_KVSTORE_WAL_H 