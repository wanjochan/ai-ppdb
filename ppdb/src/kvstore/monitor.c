#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/kvstore_monitor.h"
#include "internal/sync.h"
#include "internal/metrics.h"

// 获取CPU核心数
static int get_cpu_cores(void) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors;
}

// 获取当前时间戳（毫秒）
static time_t get_current_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 重置性能指标
static void reset_metrics(ppdb_perf_metrics_t* metrics) {
    atomic_store(&metrics->op_count, 0);
    atomic_store(&metrics->total_latency_us, 0);
    atomic_store(&metrics->max_latency_us, 0);
    atomic_store(&metrics->lock_contentions, 0);
    atomic_store(&metrics->lock_wait_us, 0);
}

ppdb_monitor_t* ppdb_monitor_create(void) {
    ppdb_monitor_t* monitor = (ppdb_monitor_t*)calloc(1, sizeof(ppdb_monitor_t));
    if (!monitor) return NULL;

    reset_metrics(&monitor->current);
    reset_metrics(&monitor->previous);
    atomic_store(&monitor->should_switch, false);
    monitor->window_start_ms = get_current_ms();
    monitor->cpu_cores = get_cpu_cores();

    return monitor;
}

void ppdb_monitor_destroy(ppdb_monitor_t* monitor) {
    if (monitor) free(monitor);
}

void ppdb_monitor_op_start(ppdb_monitor_t* monitor) {
    if (!monitor) return;

    // 检查是否需要切换窗口
    time_t current_ms = get_current_ms();
    if (current_ms - monitor->window_start_ms >= PPDB_MONITOR_WINDOW_MS) {
        // 保存当前窗口数据
        memcpy(&monitor->previous, &monitor->current, sizeof(ppdb_perf_metrics_t));
        reset_metrics(&monitor->current);
        monitor->window_start_ms = current_ms;
    }
}

void ppdb_monitor_op_end(ppdb_monitor_t* monitor, uint64_t latency_us) {
    if (!monitor) return;

    atomic_fetch_add(&monitor->current.op_count, 1);
    atomic_fetch_add(&monitor->current.total_latency_us, latency_us);

    // 更新最大延迟
    uint64_t current_max = atomic_load(&monitor->current.max_latency_us);
    while (latency_us > current_max) {
        if (atomic_compare_exchange_weak(&monitor->current.max_latency_us,
                                       &current_max, latency_us)) {
            break;
        }
    }
}

void ppdb_monitor_lock_contention(ppdb_monitor_t* monitor, uint64_t wait_us) {
    if (!monitor) return;

    atomic_fetch_add(&monitor->current.lock_contentions, 1);
    atomic_fetch_add(&monitor->current.lock_wait_us, wait_us);
}

bool ppdb_monitor_should_switch(ppdb_monitor_t* monitor) {
    if (!monitor) return false;

    // 获取当前指标
    uint64_t qps = ppdb_monitor_get_qps(monitor);
    uint64_t p99_latency = ppdb_monitor_get_p99_latency(monitor);
    double contention_rate = ppdb_monitor_get_contention_rate(monitor);

    // 判断是否需要切换到分片模式
    bool should_switch = 
        (monitor->cpu_cores >= 8) &&         // CPU核心数>=8
        ((qps > 50000) ||                    // QPS>50K
         (contention_rate > 30.0) ||         // 锁竞争率>30%
         (p99_latency > 5000));              // P99延迟>5ms

    atomic_store(&monitor->should_switch, should_switch);
    return should_switch;
}

uint64_t ppdb_monitor_get_qps(ppdb_monitor_t* monitor) {
    if (!monitor) return 0;

    uint64_t op_count = atomic_load(&monitor->current.op_count);
    time_t window_duration = get_current_ms() - monitor->window_start_ms;
    if (window_duration == 0) return 0;

    return (op_count * 1000) / window_duration;
}

uint64_t ppdb_monitor_get_p99_latency(ppdb_monitor_t* monitor) {
    if (!monitor) return 0;

    uint64_t total_latency = atomic_load(&monitor->current.total_latency_us);
    uint64_t op_count = atomic_load(&monitor->current.op_count);
    if (op_count == 0) return 0;

    // 简化的P99计算，使用最大延迟作为估计
    // 在实际生产环境中应该使用更精确的百分位数计算方法
    return atomic_load(&monitor->current.max_latency_us);
}

double ppdb_monitor_get_contention_rate(ppdb_monitor_t* monitor) {
    if (!monitor) return 0.0;

    uint64_t contentions = atomic_load(&monitor->current.lock_contentions);
    uint64_t op_count = atomic_load(&monitor->current.op_count);
    if (op_count == 0) return 0.0;

    return (double)contentions * 100.0 / op_count;
}
