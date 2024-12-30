#ifndef PPDB_TYPES_H_
#define PPDB_TYPES_H_

#include <cosmopolitan.h>

// 自旋锁类型
typedef _Atomic(int) mutex_t;

// 压缩算法类型
typedef enum ppdb_compression {
    PPDB_COMPRESSION_NONE = 0,    // 不压缩
    PPDB_COMPRESSION_SNAPPY = 1,  // Snappy压缩
    PPDB_COMPRESSION_LZ4 = 2,     // LZ4压缩
    PPDB_COMPRESSION_ZSTD = 3     // ZSTD压缩
} ppdb_compression_t;

// 运行模式
typedef enum ppdb_mode {
    PPDB_MODE_STANDALONE = 0,  // 单机模式
    PPDB_MODE_CLUSTER = 1,     // 集群模式
    PPDB_MODE_REPLICA = 2      // 复制模式
} ppdb_mode_t;

// 基础类型定义
typedef uint32_t ppdb_size_t;
typedef uint64_t ppdb_offset_t;
typedef uint64_t ppdb_timestamp_t;
typedef uint32_t ppdb_version_t;

// 常量定义
#define PPDB_MAX_KEY_SIZE 1024
#define PPDB_MAX_VALUE_SIZE (1024 * 1024)
#define PPDB_MAX_PATH_SIZE 256
#define PPDB_DEFAULT_MEMTABLE_SIZE (64 * 1024 * 1024)  // 64MB
#define PPDB_DEFAULT_WAL_SEGMENT_SIZE (4 * 1024 * 1024)  // 4MB

#endif // PPDB_TYPES_H_ 