#ifndef PPDB_TYPES_H_
#define PPDB_TYPES_H_

#include <cosmopolitan.h>

// 运行模式
typedef enum {
    PPDB_MODE_LOCKED,    // 使用互斥锁
    PPDB_MODE_LOCKFREE   // 使用无锁数据结构
} ppdb_mode_t;

// 压缩算法
typedef enum {
    PPDB_COMPRESSION_NONE,    // 不压缩
    PPDB_COMPRESSION_SNAPPY,  // Snappy压缩
    PPDB_COMPRESSION_LZ4,     // LZ4压缩
    PPDB_COMPRESSION_ZSTD     // ZSTD压缩
} ppdb_compression_t;

// 统计信息
typedef struct {
    uint64_t get_count;       // Get操作次数
    uint64_t put_count;       // Put操作次数
    uint64_t delete_count;    // Delete操作次数
    uint64_t get_hits;        // Get命中次数
    uint64_t get_misses;      // Get未命中次数
    uint64_t bytes_written;   // 写入字节数
    uint64_t bytes_read;      // 读取字节数
    uint64_t compactions;     // 压缩次数
    uint64_t merges;          // 合并次数
    uint64_t errors;          // 错误次数
} ppdb_stats_t;

#endif // PPDB_TYPES_H_ 