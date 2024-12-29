#ifndef PPDB_METRICS_H
#define PPDB_METRICS_H

#include <cosmopolitan.h>
#include <time.h>
#include "sync.h"

// 性能指标结构
typedef struct {
    ppdb_sync_t sync;              // 同步锁
    uint64_t total_ops;           // 总操作数
    uint64_t total_latency_us;    // 总延迟(微秒)
    uint32_t active_threads;      // 活跃线程数
    uint32_t max_threads;         // 最大线程数
    size_t current_size;          // 当前数据大小
    time_t last_update;           // 最后更新时间
    uint64_t last_ops;            // 上次统计的操作数
    double ops_per_sec;           // 每秒操作数
} ppdb_metrics_t;

// 初始化性能指标
void ppdb_metrics_init(ppdb_metrics_t* metrics);

// 销毁性能指标
void ppdb_metrics_destroy(ppdb_metrics_t* metrics);

// 记录操作开始
void ppdb_metrics_begin_op(ppdb_metrics_t* metrics);

// 记录操作结束
void ppdb_metrics_end_op(ppdb_metrics_t* metrics, size_t size_delta);

// 获取当前吞吐量(ops/s)
double ppdb_metrics_get_throughput(ppdb_metrics_t* metrics);

// 获取平均延迟(微秒)
double ppdb_metrics_get_avg_latency(ppdb_metrics_t* metrics);

// 获取活跃线程数
uint32_t ppdb_metrics_get_active_threads(ppdb_metrics_t* metrics);

// 获取当前数据大小
size_t ppdb_metrics_get_size(ppdb_metrics_t* metrics);

#endif // PPDB_METRICS_H
