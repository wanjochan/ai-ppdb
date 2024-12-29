#ifndef PPDB_WAL_LOCKFREE_H
#define PPDB_WAL_LOCKFREE_H

#include <cosmopolitan.h>
#include "ppdb/error.h"
#include "ppdb/memtable.h"

// WAL魔数和版本号
#define WAL_MAGIC 0x4C415750  // "PWAL"
#define WAL_VERSION 1

// WAL记录类型
typedef enum {
    PPDB_WAL_RECORD_PUT = 1,
    PPDB_WAL_RECORD_DELETE = 2
} ppdb_wal_record_type_t;

// WAL配置
typedef struct {
    const char* dir_path;    // WAL目录路径
    size_t segment_size;     // 段大小
    bool sync_write;         // 是否同步写入
} ppdb_wal_config_t;

// WAL结构
typedef struct ppdb_wal_t ppdb_wal_t;

// 创建WAL实例
ppdb_error_t ppdb_wal_create_lockfree(const ppdb_wal_config_t* config, ppdb_wal_t** wal);

// 销毁WAL实例
void ppdb_wal_destroy_lockfree(ppdb_wal_t* wal);

// 关闭WAL实例
void ppdb_wal_close_lockfree(ppdb_wal_t* wal);

// 写入记录
ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal,
                                    ppdb_wal_record_type_t type,
                                    const void* key,
                                    size_t key_size,
                                    const void* value,
                                    size_t value_size);

// 从WAL恢复数据
ppdb_error_t ppdb_wal_recover_lockfree(ppdb_wal_t* wal, ppdb_memtable_t** table);

// 归档WAL文件
ppdb_error_t ppdb_wal_archive_lockfree(ppdb_wal_t* wal);

#endif // PPDB_WAL_LOCKFREE_H 