#ifndef PPDB_WAL_H
#define PPDB_WAL_H

#include <cosmopolitan.h>
#include "ppdb/memtable.h"

// WAL 记录类型
typedef enum {
    PPDB_WAL_PUT,     // 插入/更新操作
    PPDB_WAL_DELETE   // 删除操作
} ppdb_wal_record_type_t;

// WAL 句柄
typedef struct ppdb_wal_t ppdb_wal_t;

// WAL 配置
typedef struct {
    const char* dir_path;      // WAL 文件目录
    size_t segment_size;       // 单个 WAL 文件大小
    bool sync_write;           // 是否同步写入
} ppdb_wal_config_t;

// 创建 WAL
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);

// 销毁 WAL
void ppdb_wal_destroy(ppdb_wal_t* wal);

// 写入操作记录
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, 
                           ppdb_wal_record_type_t type,
                           const uint8_t* key, size_t key_len,
                           const uint8_t* value, size_t value_len);

// 从 WAL 恢复数据到 MemTable
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* table);

// 归档旧的 WAL 文件
ppdb_error_t ppdb_wal_archive(ppdb_wal_t* wal);

#endif // PPDB_WAL_H 