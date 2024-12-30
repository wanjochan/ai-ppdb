#ifndef PPDB_METRICS_H
#define PPDB_METRICS_H

#include <cosmopolitan.h>

// 性能指标结构
typedef struct ppdb_metrics {
    atomic_uint64_t put_count;      // 写入次数
    atomic_uint64_t get_count;      // 读取次数
    atomic_uint64_t delete_count;   // 删除次数
    atomic_uint64_t total_ops;      // 总操作数
    atomic_uint64_t total_latency;  // 总延迟
} ppdb_metrics_t;

// 初始化性能指标
void ppdb_metrics_init(ppdb_metrics_t* metrics);

// 记录操作延迟
void ppdb_metrics_record_latency(ppdb_metrics_t* metrics, uint64_t latency);

// 获取平均延迟
uint64_t ppdb_metrics_avg_latency(const ppdb_metrics_t* metrics);

// 获取操作计数
uint64_t ppdb_metrics_get_count(const ppdb_metrics_t* metrics, const char* op);

#endif // PPDB_METRICS_H
