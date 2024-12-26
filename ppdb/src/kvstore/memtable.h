#ifndef PPDB_MEMTABLE_H
#define PPDB_MEMTABLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>

// ... existing code ...

// 监控指标结构体
typedef struct {
    atomic_size_t current_memory_usage;    // 当前内存使用量
    atomic_size_t peak_memory_usage;       // 峰值内存使用
    atomic_uint64_t total_operations;      // 总操作数
    atomic_uint64_t write_operations;      // 写操作数
    atomic_uint64_t read_operations;       // 读操作数
    atomic_uint64_t write_conflicts;       // 写冲突次数
    atomic_uint64_t memory_warnings;       // 内存警告次数
    atomic_uint64_t flush_triggers;        // 触发刷盘次数
} ppdb_memtable_metrics_t;

// 扩展 MemTable 结构体
typedef struct ppdb_memtable_t {
    ppdb_skiplist_t* list;                // 跳表实现
    size_t max_size;                      // 最大大小
    atomic_size_t current_size;           // 当前大小
    pthread_mutex_t mutex;                // 并发控制
    ppdb_memtable_metrics_t metrics;      // 监控指标
    double warning_threshold;             // 内存警告阈值（默认80%）
    double critical_threshold;            // 内存临界阈值（默认90%）
} ppdb_memtable_t;

// ... existing code ...

#endif // PPDB_MEMTABLE_H 