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
    uint64_t next_sequence;
    wal_buffer_t* buffers;
    size_t buffer_count;
    ppdb_metrics_t metrics;
} ppdb_wal_t;

// WAL 配置
typedef struct ppdb_wal_config {
    const char* filename;
    bool sync_on_write;
    bool enable_compression;
    size_t buffer_size;
    size_t buffer_count;
} ppdb_wal_config_t;

// WAL 恢复迭代器
typedef struct ppdb_wal_recovery_iter {
    ppdb_wal_t* wal;
    off_t position;
    void* buffer;
    size_t buffer_size;
    struct {
        void* key;
        size_t key_size;
        void* value;
        size_t value_size;
    } current;
} ppdb_wal_recovery_iter_t;

// 基础 WAL 操作
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal);

// 无锁 WAL 操作
ppdb_error_t ppdb_wal_write_lockfree_basic(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync_lockfree_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_lockfree_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_lockfree_basic(ppdb_wal_t* wal);

// WAL 恢复操作
ppdb_error_t ppdb_wal_recovery_iter_create_basic(ppdb_wal_t* wal, ppdb_wal_recovery_iter_t** iter);
void ppdb_wal_recovery_iter_destroy_basic(ppdb_wal_recovery_iter_t* iter);
ppdb_error_t ppdb_wal_recovery_iter_next_basic(ppdb_wal_recovery_iter_t* iter);
ppdb_error_t ppdb_wal_recover_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable);
ppdb_error_t ppdb_wal_recover_lockfree_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable);

// 工厂函数
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal);
size_t ppdb_wal_size(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* memtable);

#endif // PPDB_KVSTORE_WAL_H 