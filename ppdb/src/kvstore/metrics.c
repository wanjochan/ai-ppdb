#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/metrics.h"

// Get current timestamp (microseconds)
static uint64_t get_timestamp_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

// Initialize performance metrics
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

// Reset performance metrics
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

// Record operation
void ppdb_metrics_record_op(ppdb_metrics_t* metrics, uint64_t latency_us) {
    if (!metrics) return;
    
    atomic_fetch_add(&metrics->total_ops, 1);
    atomic_fetch_add(&metrics->total_latency_us, latency_us);
    
    // Update max latency
    uint64_t old_max;
    do {
        old_max = atomic_load(&metrics->max_latency_us);
        if (latency_us <= old_max) break;
    } while (!atomic_compare_exchange_weak(&metrics->max_latency_us, &old_max, latency_us));
    
    // Update min latency
    uint64_t old_min;
    do {
        old_min = atomic_load(&metrics->min_latency_us);
        if (latency_us >= old_min) break;
    } while (!atomic_compare_exchange_weak(&metrics->min_latency_us, &old_min, latency_us));
}

// Record data
void ppdb_metrics_record_data(ppdb_metrics_t* metrics, size_t key_size, size_t value_size) {
    if (!metrics) return;
    
    atomic_fetch_add(&metrics->total_bytes, key_size + value_size);
    atomic_fetch_add(&metrics->total_keys, 1);
    atomic_fetch_add(&metrics->total_values, 1);
}

// Get average latency
uint64_t ppdb_metrics_avg_latency(ppdb_metrics_t* metrics) {
    if (!metrics) return 0;
    
    uint64_t total_ops = atomic_load(&metrics->total_ops);
    if (total_ops == 0) return 0;
    
    uint64_t total_latency = atomic_load(&metrics->total_latency_us);
    return total_latency / total_ops;
}

// Get total operations
uint64_t ppdb_metrics_total_ops(ppdb_metrics_t* metrics) {
    if (!metrics) return 0;
    return atomic_load(&metrics->total_ops);
}

// Get total bytes
uint64_t ppdb_metrics_total_bytes(ppdb_metrics_t* metrics) {
    if (!metrics) return 0;
    return atomic_load(&metrics->total_bytes);
}
