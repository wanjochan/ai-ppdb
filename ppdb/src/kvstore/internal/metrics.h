#ifndef PPDB_KVSTORE_INTERNAL_METRICS_H
#define PPDB_KVSTORE_INTERNAL_METRICS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"

// 性能监控结构
typedef struct ppdb_metrics {
    atomic_uint64_t total_ops;         // 总操作数
    atomic_uint64_t total_latency_us;  // 总延迟(微秒)
    atomic_uint64_t max_latency_us;    // 最大延迟(微秒)
    atomic_uint64_t min_latency_us;    // 最小延迟(微秒)
    atomic_uint64_t total_bytes;       // 总字节数
    atomic_uint64_t total_keys;        // 总键数
    atomic_uint64_t total_values;      // 总值数
} ppdb_metrics_t;

// 初始化性能监控
void ppdb_metrics_init(ppdb_metrics_t* metrics);

// 重置性能监控
void ppdb_metrics_reset(ppdb_metrics_t* metrics);

// 记录操作
void ppdb_metrics_record_op(ppdb_metrics_t* metrics, uint64_t latency_us);

// 记录数据
void ppdb_metrics_record_data(ppdb_metrics_t* metrics,
                            size_t key_size,
                            size_t value_size);

// 获取平均延迟
uint64_t ppdb_metrics_avg_latency(ppdb_metrics_t* metrics);

// 获取操作数
uint64_t ppdb_metrics_total_ops(ppdb_metrics_t* metrics);

// 获取总字节数
uint64_t ppdb_metrics_total_bytes(ppdb_metrics_t* metrics);

#endif // PPDB_KVSTORE_INTERNAL_METRICS_H
