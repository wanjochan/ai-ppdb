#ifndef PPDB_KVSTORE_WAL_TYPES_H
#define PPDB_KVSTORE_WAL_TYPES_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/sync.h"

// WAL魔数和版本
#define WAL_MAGIC 0x4C415750  // "PWAL" in little-endian
#define WAL_VERSION 1

// WAL记录类型
typedef enum {
    PPDB_WAL_RECORD_PUT = 1,
    PPDB_WAL_RECORD_DELETE = 2
} ppdb_wal_record_type_t;

// WAL头部
typedef struct {
    uint32_t magic;      // WAL文件魔数
    uint32_t version;    // WAL版本号
    uint64_t sequence;   // 序列号
} wal_header_t;

// WAL记录头部
typedef struct {
    uint8_t type;        // 记录类型
    uint32_t key_size;   // 键大小
    uint32_t value_size; // 值大小
    uint32_t crc32;      // CRC32校验和
} wal_record_header_t;

// WAL缓冲区
typedef struct {
    void* data;          // 缓冲区数据
    size_t size;         // 缓冲区大小
    size_t used;         // 已使用大小
    bool in_use;         // 是否在使用
    ppdb_sync_t sync;    // 同步对象
} wal_buffer_t;

// WAL当前记录
typedef struct {
    void* key;           // 键数据
    size_t key_size;     // 键大小
    void* value;         // 值数据
    size_t value_size;   // 值大小
} wal_record_t;

// WAL结构体
typedef struct ppdb_wal {
    char* dir_path;              // WAL目录路径
    char* filename;              // WAL文件名
    int current_fd;             // 当前文件描述符
    size_t current_size;        // 当前文件大小
    uint64_t next_sequence;     // 下一个序列号
    bool sync_on_write;         // 是否在写入时同步
    bool closed;                // 是否已关闭
    ppdb_sync_t* sync;          // 同步对象
} ppdb_wal_t;

// WAL恢复迭代器
typedef struct ppdb_wal_recovery_iter {
    ppdb_wal_t* wal;            // WAL对象
    int fd;                     // 当前段文件描述符
    size_t offset;              // 当前读取位置
    void* buffer;               // 读取缓冲区
    size_t buffer_size;         // 缓冲区大小
    off_t position;             // 当前位置
    wal_record_t current;       // 当前记录
} ppdb_wal_recovery_iter_t;

#endif // PPDB_KVSTORE_WAL_TYPES_H