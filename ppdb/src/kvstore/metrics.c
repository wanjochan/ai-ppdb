#include <cosmopolitan.h>
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
    
    ppdb_sync_init(&metrics->sync);
    metrics->total_ops = 0;
    metrics->total_latency_us = 0;
    metrics->active_threads = 0;
    metrics->max_threads = 0;
    metrics->current_size = 0;
    metrics->last_update = time(NULL);
    metrics->last_ops = 0;
    metrics->ops_per_sec = 0.0;
}

// Destroy performance metrics
void ppdb_metrics_destroy(ppdb_metrics_t* metrics) {
    if (!metrics) return;
    ppdb_sync_destroy(&metrics->sync);
}

// Record operation start
void ppdb_metrics_begin_op(ppdb_metrics_t* metrics) {
    if (!metrics) return;
    
    ppdb_sync_lock(&metrics->sync);
    metrics->active_threads++;
    if (metrics->active_threads > metrics->max_threads) {
        metrics->max_threads = metrics->active_threads;
    }
    ppdb_sync_unlock(&metrics->sync);
}

// Record operation end
void ppdb_metrics_end_op(ppdb_metrics_t* metrics, size_t size_delta) {
    if (!metrics) return;
    
    static const time_t UPDATE_INTERVAL = 1; // Update throughput every 1 second
    
    uint64_t end_time = get_timestamp_us();
    time_t current_time = time(NULL);
    
    ppdb_sync_lock(&metrics->sync);
    
    // Update operation count and size
    metrics->total_ops++;
    metrics->current_size += size_delta;
    metrics->active_threads--;
    
    // Update throughput if interval has elapsed
    time_t time_diff = current_time - metrics->last_update;
    if (time_diff >= UPDATE_INTERVAL) {
        uint64_t ops_diff = metrics->total_ops - metrics->last_ops;
        metrics->ops_per_sec = (double)ops_diff / time_diff;
        metrics->last_update = current_time;
        metrics->last_ops = metrics->total_ops;
    }
    
    ppdb_sync_unlock(&metrics->sync);
}

// Get current throughput
double ppdb_metrics_get_throughput(ppdb_metrics_t* metrics) {
    if (!metrics) return 0.0;
    
    ppdb_sync_lock(&metrics->sync);
    double throughput = metrics->ops_per_sec;
    ppdb_sync_unlock(&metrics->sync);
    
    return throughput;
}

// Get average latency
double ppdb_metrics_get_avg_latency(ppdb_metrics_t* metrics) {
    if (!metrics) return 0.0;
    
    ppdb_sync_lock(&metrics->sync);
    double avg_latency = metrics->total_ops > 0 
        ? (double)metrics->total_latency_us / metrics->total_ops 
        : 0.0;
    ppdb_sync_unlock(&metrics->sync);
    
    return avg_latency;
}

// Get active thread count
uint32_t ppdb_metrics_get_active_threads(ppdb_metrics_t* metrics) {
    if (!metrics) return 0;
    
    ppdb_sync_lock(&metrics->sync);
    uint32_t threads = metrics->active_threads;
    ppdb_sync_unlock(&metrics->sync);
    
    return threads;
}

// Get current data size
size_t ppdb_metrics_get_size(ppdb_metrics_t* metrics) {
    if (!metrics) return 0;
    
    ppdb_sync_lock(&metrics->sync);
    size_t size = metrics->current_size;
    ppdb_sync_unlock(&metrics->sync);
    
    return size;
}