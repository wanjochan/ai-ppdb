#ifndef PPDB_KVSTORE_WAL_TYPES_H
#define PPDB_KVSTORE_WAL_TYPES_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_types.h"

// WAL 常量定义
#define WAL_MAGIC 0x4C415750  // "PWAL"
#define WAL_VERSION 1
#define WAL_BUFFER_SIZE (64 * 1024)  // 64KB
#define WAL_SEGMENT_HEADER_SIZE sizeof(wal_segment_header_t)
#define WAL_RECORD_HEADER_SIZE sizeof(wal_record_header_t)

// WAL 记录类型
typedef enum ppdb_wal_record_type {
    PPDB_WAL_RECORD_PUT = 1,
    PPDB_WAL_RECORD_DELETE = 2,
    PPDB_WAL_RECORD_MERGE = 3
} ppdb_wal_record_type_t;

// WAL 段头部
typedef struct wal_segment_header {
    uint32_t magic;           // 魔数
    uint32_t version;         // 版本号
    uint64_t first_sequence;  // 第一条记录序号
    uint64_t last_sequence;   // 最后一条记录序号
    uint32_t record_count;    // 记录数量
    uint32_t checksum;        // 校验和
} wal_segment_header_t;

// WAL 记录头部
typedef struct wal_record_header {
    uint32_t magic;           // 魔数
    uint32_t type;           // 记录类型
    uint32_t key_size;       // 键大小
    uint32_t value_size;     // 值大小
    uint64_t sequence;       // 序号
    uint32_t checksum;       // 校验和
} wal_record_header_t;

// WAL 段
typedef struct wal_segment {
    uint64_t id;             // 段ID
    char* filename;          // 文件名
    int fd;                  // 文件描述符
    size_t size;            // 段大小
    bool is_sealed;         // 是否已封存
    uint64_t first_sequence; // 第一条记录序号
    uint64_t last_sequence;  // 最后一条记录序号
    struct wal_segment* next; // 下一个段
} wal_segment_t;

// WAL 结构
typedef struct ppdb_wal {
    ppdb_wal_config_t config;  // 配置
    char* dir_path;            // 目录路径
    wal_segment_t* segments;   // 段链表
    size_t segment_count;      // 段数量
    uint64_t next_sequence;    // 下一个序号
    uint64_t next_segment_id;  // 下一个段ID
    int current_fd;            // 当前文件描述符
    size_t current_size;       // 当前段大小
    size_t total_size;         // 总大小
    void* write_buffer;        // 写入缓冲区
    bool closed;               // 是否已关闭
    bool sync_on_write;        // 是否同步写入
    ppdb_sync_t* sync;         // 同步原语
} ppdb_wal_t;

#endif // PPDB_KVSTORE_WAL_TYPES_H 