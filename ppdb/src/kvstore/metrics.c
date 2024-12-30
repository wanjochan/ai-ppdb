#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/metrics.h"

// 获取当前时间戳(微秒)
uint64_t now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

// 初始化性能监控
void ppdb_metrics_init(ppdb_metrics_t* metrics) {
    if (!metrics) return;
    
    atomic_init(&metrics->total_ops, 0);
    atomic_init(&metrics->total_latency_us, 0);
    atomic_init(&metrics->max_latency_us, 0);
    atomic_init(&metrics->min_latency_us, UINT64_MAX);
    atomic_init(&metrics->total_bytes, 0);
    atomic_init(&metrics->total_keys, 0);
    atomic_init(&metrics->total_values, 0);
}

// 重置性能监控
void ppdb_metrics_reset(ppdb_metrics_t* metrics) {
    if (!metrics) return;
    
    atomic_store(&metrics->total_ops, 0);
    atomic_store(&metrics->total_latency_us, 0);
    atomic_store(&metrics->max_latency_us, 0);
    atomic_store(&metrics->min_latency_us, UINT64_MAX);
    atomic_store(&metrics->total_bytes, 0);
    atomic_store(&metrics->total_keys, 0);
    atomic_store(&metrics->total_values, 0);
}

// 记录操作
void ppdb_metrics_record_op(ppdb_metrics_t* metrics, uint64_t latency_us) {
    if (!metrics) return;
    
    atomic_fetch_add(&metrics->total_ops, 1);
    atomic_fetch_add(&metrics->total_latency_us, latency_us);
    
    uint64_t current_max = atomic_load(&metrics->max_latency_us);
    while (latency_us > current_max) {
        if (atomic_compare_exchange_weak(&metrics->max_latency_us,
                                       &current_max, latency_us)) {
            break;
        }
    }
    
    uint64_t current_min = atomic_load(&metrics->min_latency_us);
    while (latency_us < current_min) {
        if (atomic_compare_exchange_weak(&metrics->min_latency_us,
                                       &current_min, latency_us)) {
            break;
        }
    }
}

// 记录数据
void ppdb_metrics_record_data(ppdb_metrics_t* metrics, size_t key_size, size_t value_size) {
    if (!metrics) return;
    
    atomic_fetch_add(&metrics->total_bytes, key_size + value_size);
    atomic_fetch_add(&metrics->total_keys, 1);
    if (value_size > 0) {
        atomic_fetch_add(&metrics->total_values, 1);
    }
}

// 获取平均延迟
uint64_t ppdb_metrics_avg_latency(ppdb_metrics_t* metrics) {
    if (!metrics) return 0;
    
    uint64_t total_ops = atomic_load(&metrics->total_ops);
    if (total_ops == 0) return 0;
    
    return atomic_load(&metrics->total_latency_us) / total_ops;
}

// 获取操作数
uint64_t ppdb_metrics_total_ops(ppdb_metrics_t* metrics) {
    if (!metrics) return 0;
    return atomic_load(&metrics->total_ops);
}

// 获取总字节数
uint64_t ppdb_metrics_total_bytes(ppdb_metrics_t* metrics) {
    if (!metrics) return 0;
    return atomic_load(&metrics->total_bytes);
}
