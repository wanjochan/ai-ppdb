#ifndef PPDB_WAL_H
#define PPDB_WAL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "ppdb/error.h"
#include "ppdb/defs.h"

// WAL配置结构
typedef struct {
    char dir_path[256];  // WAL目录路径
    size_t segment_size; // 段大小
    bool sync_write;     // 是否同步写入
    ppdb_mode_t mode;    // 运行模式
} ppdb_wal_config_t;

// WAL记录类型
typedef enum {
    PPDB_WAL_RECORD_PUT,     // 写入记录
    PPDB_WAL_RECORD_DELETE   // 删除记录
} ppdb_wal_record_type_t;

// WAL结构(不透明)
typedef struct ppdb_wal_t ppdb_wal_t;

// 创建WAL实例
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);

// 销毁WAL实例
void ppdb_wal_destroy(ppdb_wal_t* wal);

// 关闭WAL实例
void ppdb_wal_close(ppdb_wal_t* wal);

// 写入记录
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal,
                           ppdb_wal_record_type_t type,
                           const void* key,
                           size_t key_size,
                           const void* value,
                           size_t value_size);

// 从WAL恢复数据
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, struct ppdb_memtable_t** table);

// 归档WAL文件
ppdb_error_t ppdb_wal_archive(ppdb_wal_t* wal);

#endif // PPDB_WAL_H