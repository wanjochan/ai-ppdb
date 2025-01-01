#ifndef PPDB_KVSTORE_WAL_TYPES_H
#define PPDB_KVSTORE_WAL_TYPES_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_wal.h"
#include "kvstore/internal/sync.h"

// WAL 魔数和版本
#define WAL_MAGIC 0x4C415750  // "PWAL" in little-endian
#define WAL_VERSION 1

// WAL 缓冲区和头部大小
#define WAL_BUFFER_SIZE 4096
#define WAL_SEGMENT_HEADER_SIZE sizeof(wal_segment_header_t)

// WAL 记录类型
typedef enum {
    PPDB_WAL_RECORD_PUT = 1,
    PPDB_WAL_RECORD_DELETE = 2
} ppdb_wal_record_type_t;

// WAL 段头部
typedef struct {
    uint32_t magic;           // WAL 魔数
    uint32_t version;         // WAL 版本号
    uint64_t id;             // 段 ID
    uint64_t first_sequence; // 第一个记录的序列号
    uint64_t last_sequence;  // 最后一个记录的序列号
    uint32_t record_count;   // 记录数量
    uint32_t checksum;       // 校验和
} wal_segment_header_t;

// WAL 记录头部
typedef struct {
    uint32_t magic;          // 记录魔数
    uint8_t type;           // 记录类型
    uint32_t key_size;      // 键大小
    uint32_t value_size;    // 值大小
    uint64_t sequence;      // 序列号
    uint32_t checksum;      // 校验和
} wal_record_header_t;

// WAL 段结构
typedef struct wal_segment {
    uint64_t id;                  // 段 ID
    char* filename;               // 段文件名
    int fd;                      // 文件描述符
    size_t size;                 // 段大小
    struct wal_segment* next;    // 下一个段
    bool is_sealed;              // 是否已封存
    uint64_t first_sequence;     // 第一个记录的序列号
    uint64_t last_sequence;      // 最后一个记录的序列号
} wal_segment_t;

// WAL 结构体
typedef struct ppdb_wal {
    ppdb_wal_config_t config;    // WAL 配置
    char* dir_path;              // WAL 目录路径
    wal_segment_t* segments;     // 段链表
    size_t segment_count;        // 段数量
    uint64_t next_sequence;      // 下一个序列号
    uint64_t next_segment_id;    // 下一个段 ID
    int current_fd;              // 当前文件描述符
    size_t current_size;         // 当前大小
    void* write_buffer;          // 写入缓冲区
    bool closed;                 // 是否已关闭
    bool sync_on_write;          // 是否同步写入
    ppdb_sync_t* sync;          // 同步对象
} ppdb_wal_t;

// WAL 迭代器结构
typedef struct ppdb_wal_iterator {
    ppdb_wal_t* wal;            // WAL 实例
    wal_segment_t* curr_segment; // 当前段
    size_t curr_offset;          // 当前段内偏移
    bool valid;                  // 是否有效
    void* read_buffer;          // 读取缓冲区
    size_t buffer_size;         // 缓冲区大小
    uint64_t last_sequence;     // 最后读取的序列号
} ppdb_wal_iterator_t;

#endif // PPDB_KVSTORE_WAL_TYPES_H