#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/kvstore_monitor.h"
#include "internal/metrics.h"

// 监控窗口大小（毫秒）
#define PPDB_MONITOR_WINDOW_MS 1000

// 监控器结构
struct ppdb_monitor {
    ppdb_metrics_t current;     // 当前窗口指标
    ppdb_metrics_t previous;    // 上一窗口指标
    uint64_t window_start_ms;   // 窗口开始时间
    uint32_t cpu_cores;         // CPU核心数
};

// 获取CPU核心数
static uint32_t get_cpu_cores(void) {
    struct NtSystemInfo sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors;
}

// 获取当前时间（毫秒）
static uint64_t get_current_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// 创建监控器
ppdb_error_t ppdb_monitor_create(ppdb_monitor_t** monitor) {
    if (!monitor) return PPDB_ERR_NULL_POINTER;

    ppdb_monitor_t* new_monitor = (ppdb_monitor_t*)calloc(1, sizeof(ppdb_monitor_t));
    if (!new_monitor) return PPDB_ERR_OUT_OF_MEMORY;

    ppdb_metrics_init(&new_monitor->current);
    ppdb_metrics_init(&new_monitor->previous);
    new_monitor->window_start_ms = get_current_ms();
    new_monitor->cpu_cores = get_cpu_cores();

    *monitor = new_monitor;
    return PPDB_OK;
}

// 销毁监控器
void ppdb_monitor_destroy(ppdb_monitor_t* monitor) {
    if (!monitor) return;
    free(monitor);
}

// 操作开始
void ppdb_monitor_op_start(ppdb_monitor_t* monitor) {
    if (!monitor) return;

    uint64_t current_ms = get_current_ms();

    // 检查是否需要切换窗口
    if (current_ms - monitor->window_start_ms >= PPDB_MONITOR_WINDOW_MS) {
        // 切换窗口
        monitor->previous = monitor->current;
        ppdb_metrics_reset(&monitor->current);
        monitor->window_start_ms = current_ms;
    }
}

// 操作结束
void ppdb_monitor_op_end(ppdb_monitor_t* monitor, uint64_t latency_us) {
    if (!monitor) return;
    ppdb_metrics_record_op(&monitor->current, latency_us);
}

// 是否应该切换内存表
bool ppdb_monitor_should_switch(ppdb_monitor_t* monitor) {
    if (!monitor) return false;

    // 获取当前性能指标
    uint64_t ops = ppdb_metrics_total_ops(&monitor->current);
    uint64_t avg_latency = ppdb_metrics_avg_latency(&monitor->current);

    // 如果操作数太少，不切换
    if (ops < 1000) return false;

    // 如果平均延迟超过10ms，建议切换
    if (avg_latency > 10000) return true;

    return false;
}

// 获取操作数
uint64_t ppdb_monitor_get_op_count(ppdb_monitor_t* monitor) {
    if (!monitor) return 0;
    return ppdb_metrics_total_ops(&monitor->current);
}

// 获取平均延迟
uint64_t ppdb_monitor_get_avg_latency(ppdb_monitor_t* monitor) {
    if (!monitor) return 0;
    return ppdb_metrics_avg_latency(&monitor->current);
}

// 获取内存使用量
uint64_t ppdb_monitor_get_memory_usage(ppdb_monitor_t* monitor) {
    if (!monitor) return 0;
    return ppdb_metrics_total_bytes(&monitor->current);
}
