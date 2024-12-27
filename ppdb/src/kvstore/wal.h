#ifndef PPDB_WAL_H
#define PPDB_WAL_H

#include <stdint.h>
#include <stdbool.h>
#include "memtable.h"

// WAL记录类型
typedef enum {
    PPDB_WAL_PUT = 1,    // 写入记录
    PPDB_WAL_DELETE = 2  // 删除记录
} ppdb_wal_record_type_t;

// WAL配置
typedef struct {
    const char* dir_path;     // WAL目录路径
    size_t segment_size;      // 段大小
    bool sync_write;          // 是否同步写入
} ppdb_wal_config_t;

// WAL结构（不透明类型）
typedef struct ppdb_wal_t ppdb_wal_t;

/**
 * 创建WAL实例
 * @param config WAL配置
 * @param wal 返回的WAL实例
 * @return 错误码
 */
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);

/**
 * 销毁WAL实例
 * @param wal WAL实例
 */
void ppdb_wal_destroy(ppdb_wal_t* wal);

/**
 * 写入记录
 * @param wal WAL实例
 * @param type 记录类型
 * @param key 键
 * @param key_size 键大小
 * @param value 值
 * @param value_size 值大小
 * @return 错误码
 */
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal,
                           ppdb_wal_record_type_t type,
                           const void* key,
                           size_t key_size,
                           const void* value,
                           size_t value_size);

/**
 * 恢复数据
 * @param wal WAL实例
 * @param table 内存表
 * @return 错误码
 */
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* table);

/**
 * 归档WAL文件
 * @param wal WAL实例
 * @return 错误码
 */
ppdb_error_t ppdb_wal_archive(ppdb_wal_t* wal);

#endif // PPDB_WAL_H