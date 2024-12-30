#ifndef PPDB_METRICS_H
#define PPDB_METRICS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_types.h"

typedef struct ppdb_metrics {
    atomic_uint64_t put_count;      // 写入次数
    atomic_uint64_t get_count;      // 读取次数
    atomic_uint64_t delete_count;   // 删除次数
    atomic_uint64_t total_ops;      // 总操作数
    atomic_uint64_t total_latency;  // 总延迟
    atomic_uint64_t total_latency_us;  // 微秒级总延迟
    atomic_uint64_t max_latency_us;    // 最大延迟
    atomic_uint64_t min_latency_us;    // 最小延迟
    atomic_uint64_t total_bytes;       // 总字节数
    atomic_uint64_t total_keys;        // 总键数
    atomic_uint64_t total_values;      // 总值数
} ppdb_metrics_t;

// 初始化指标
void ppdb_metrics_init(ppdb_metrics_t* metrics);

// 重置指标
void ppdb_metrics_reset(ppdb_metrics_t* metrics);

// 记录操作延迟
void ppdb_metrics_record_op(ppdb_metrics_t* metrics, uint64_t latency_us);

// 记录数据大小
void ppdb_metrics_record_data(ppdb_metrics_t* metrics, size_t key_size, size_t value_size);

// 获取平均延迟
uint64_t ppdb_metrics_avg_latency(const ppdb_metrics_t* metrics);

// 获取总操作数
uint64_t ppdb_metrics_total_ops(const ppdb_metrics_t* metrics);

// 获取总字节数
uint64_t ppdb_metrics_total_bytes(const ppdb_metrics_t* metrics);

#endif // PPDB_METRICS_H
