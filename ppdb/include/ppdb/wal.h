#ifndef PPDB_WAL_H
#define PPDB_WAL_H

#include <cosmopolitan.h>
#include "defs.h"
#include "memtable.h"

// WAL常量
#define WAL_MAGIC 0x4C415750  // "PWAL"
#define WAL_VERSION 1
#define MAX_KEY_SIZE (1024 * 1024)     // 1MB
#define MAX_VALUE_SIZE (10 * 1024 * 1024)  // 10MB

// WAL配置
typedef struct ppdb_wal_config_t {
    const char* dir_path;  // WAL目录路径
    size_t segment_size;   // WAL段文件大小限制
    bool sync_write;       // 是否同步写入
} ppdb_wal_config_t;

// WAL实例
typedef struct ppdb_wal_t {
    char dir_path[MAX_PATH_LENGTH];  // WAL目录路径
    size_t segment_size;             // WAL段文件大小限制
    bool sync_write;                 // 是否同步写入
    int current_fd;                  // 当前WAL段文件描述符
    size_t current_size;             // 当前WAL段文件大小
    pthread_mutex_t mutex;           // 互斥锁
} ppdb_wal_t;

// WAL文件头
typedef struct ppdb_wal_header_t {
    uint32_t magic;       // 魔数
    uint32_t version;     // 版本号
    uint32_t segment_size; // 段文件大小限制
    uint32_t reserved;    // 保留字段
} ppdb_wal_header_t;

// WAL记录类型
typedef enum ppdb_wal_record_type_t {
    PPDB_WAL_RECORD_PUT = 1,  // 写入记录
    PPDB_WAL_RECORD_DELETE = 2 // 删除记录
} ppdb_wal_record_type_t;

// WAL记录头
typedef struct ppdb_wal_record_header_t {
    uint32_t type;      // 记录类型
    uint32_t key_size;  // 键大小
    uint32_t value_size; // 值大小
} ppdb_wal_record_header_t;

// 创建WAL实例
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config,
                            ppdb_wal_t** wal);

// 销毁WAL实例
void ppdb_wal_destroy(ppdb_wal_t* wal);

// 写入记录
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal,
                           ppdb_wal_record_type_t type,
                           const void* key,
                           size_t key_size,
                           const void* value,
                           size_t value_size);

// 恢复数据
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal,
                             ppdb_memtable_t* table);

// 归档WAL文件
ppdb_error_t ppdb_wal_archive(ppdb_wal_t* wal);

// 关闭WAL实例
void ppdb_wal_close(ppdb_wal_t* wal);

#endif // PPDB_WAL_H