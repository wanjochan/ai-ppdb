#ifndef PPDB_MONITOR_H
#define PPDB_MONITOR_H

#include <cosmopolitan.h>
#include "sync.h"
#include <stdatomic.h>
#include <time.h>

// 性能监控窗口大小（毫秒）
#define PPDB_MONITOR_WINDOW_MS 1000

// 性能指标结构
typedef struct ppdb_perf_metrics {
    atomic_uint_least64_t op_count;           // 操作总数
    atomic_uint_least64_t total_latency_us;   // 总延迟（微秒）
    atomic_uint_least64_t max_latency_us;     // 最大延迟（微秒）
    atomic_uint_least64_t lock_contentions;   // 锁竞争次数
    atomic_uint_least64_t lock_wait_us;       // 锁等待时间
} ppdb_perf_metrics_t;

// 性能监控器结构
typedef struct ppdb_monitor {
    ppdb_perf_metrics_t current;              // 当前窗口指标
    ppdb_perf_metrics_t previous;             // 上一窗口指标
    atomic_bool should_switch;                // 是否需要切换标志
    time_t window_start_ms;                   // 当前窗口开始时间
    int cpu_cores;                            // CPU核心数
} ppdb_monitor_t;

// 创建监控器
ppdb_monitor_t* ppdb_monitor_create(void);

// 销毁监控器
void ppdb_monitor_destroy(ppdb_monitor_t* monitor);

// 记录操作开始
void ppdb_monitor_op_start(ppdb_monitor_t* monitor);

// 记录操作结束
void ppdb_monitor_op_end(ppdb_monitor_t* monitor, uint64_t latency_us);

// 记录锁竞争
void ppdb_monitor_lock_contention(ppdb_monitor_t* monitor, uint64_t wait_us);

// 检查是否需要切换到分片模式
bool ppdb_monitor_should_switch(ppdb_monitor_t* monitor);

// 获取当前QPS
uint64_t ppdb_monitor_get_qps(ppdb_monitor_t* monitor);

// 获取当前P99延迟（微秒）
uint64_t ppdb_monitor_get_p99_latency(ppdb_monitor_t* monitor);

// 获取锁竞争率（百分比）
double ppdb_monitor_get_contention_rate(ppdb_monitor_t* monitor);

#endif // PPDB_MONITOR_H
