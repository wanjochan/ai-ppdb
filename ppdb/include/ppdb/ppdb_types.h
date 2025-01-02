#ifndef PPDB_TYPES_H_
#define PPDB_TYPES_H_

#include <cosmopolitan.h>

// 最大路径长度
#define PPDB_MAX_PATH_SIZE 256

// 压缩类型
typedef enum {
    PPDB_COMPRESSION_NONE = 0,
    PPDB_COMPRESSION_SNAPPY,
    PPDB_COMPRESSION_LZ4,
    PPDB_COMPRESSION_ZSTD
} ppdb_compression_t;

// 键值对结构
typedef struct {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
} ppdb_kv_pair_t;

// 比较函数类型
typedef int (*ppdb_compare_func_t)(const void* a, size_t a_len, const void* b, size_t b_len);

// 跳表结构
typedef struct ppdb_skiplist ppdb_skiplist_t;

// 跳表迭代器结构
typedef struct ppdb_skiplist_iterator ppdb_skiplist_iterator_t;

// 内存表结构
typedef struct ppdb_memtable ppdb_memtable_t;

// 内存表迭代器结构
typedef struct ppdb_memtable_iterator ppdb_memtable_iterator_t;

// 内存表类型
typedef enum {
    PPDB_MEMTABLE_BASIC = 0,
    PPDB_MEMTABLE_SHARDED,
    PPDB_MEMTABLE_LOCKFREE
} ppdb_memtable_type_t;

// 性能指标结构
typedef struct ppdb_metrics {
    atomic_size_t put_count;         // 写入次数
    atomic_size_t get_count;         // 读取次数
    atomic_size_t delete_count;      // 删除次数
    atomic_size_t total_ops;         // 总操作次数
    atomic_size_t total_latency;     // 总延迟
    atomic_size_t total_latency_us;  // 总延迟（微秒）
    atomic_size_t max_latency_us;    // 最大延迟（微秒）
    atomic_size_t min_latency_us;    // 最小延迟（微秒）
    atomic_size_t total_bytes;       // 总字节数
    atomic_size_t total_keys;        // 总键数
    atomic_size_t total_values;      // 总值数
    atomic_size_t bytes_written;     // 写入字节数
    atomic_size_t bytes_read;        // 读取字节数
    atomic_size_t get_miss_count;    // 读取未命中次数
} ppdb_metrics_t;

#endif // PPDB_TYPES_H_ 