#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"

#define TARGET_CONNECTIONS 1000
#define BATCH_SIZE 100
#define TEST_DURATION_SEC 30
#define TASK_LIFETIME_MS 1000  // 每个任务保持活跃1秒
#define MAX_YIELD_COUNT 500000  // 增加yield次数限制

// Performance metrics
typedef struct {
    size_t active_tasks;
    size_t completed_tasks;
    size_t failed_tasks;
    double avg_response_time;
    size_t peak_memory;
    struct timespec start_time;
    double cpu_usage;
    size_t total_memory;
    size_t peak_active_tasks;  // 新增：记录峰值并发数
} TestMetrics;

TestMetrics metrics = {0};

// Get CPU usage
double get_cpu_usage() {
    static clock_t last_cpu = 0;
    static struct timespec last_time = {0};
    
    clock_t current_cpu = clock();
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    if (last_cpu == 0) {
        last_cpu = current_cpu;
        last_time = current_time;
        return 0.0;
    }
    
    double cpu_time = (double)(current_cpu - last_cpu) / CLOCKS_PER_SEC;
    double real_time = (current_time.tv_sec - last_time.tv_sec) + 
                      (current_time.tv_nsec - last_time.tv_nsec) / 1e9;
    
    last_cpu = current_cpu;
    last_time = current_time;
    
    return (cpu_time / real_time) * 100.0;
}

// Task context structure
typedef struct {
    InfraxAsync* async;
    struct timespec start_time;
    int is_active;
    int state;  // 0: initial, 1: running, 2: completed
} TaskContext;

// Task cleanup function
void cleanup_task(TaskContext* ctx) {
    if (ctx) {
        if (ctx->async) {
            InfraxAsyncClass.free(ctx->async); // 修正：使用正确的释放函数
        }
        free(ctx);
        __atomic_fetch_sub(&metrics.active_tasks, 1, __ATOMIC_SEQ_CST);
    }
}

// Long running task function
void long_running_task(InfraxAsync* self, void* arg) {
    TaskContext* ctx = (TaskContext*)arg;
    static __thread int yield_count = 0;
    static __thread struct timespec last_yield_time = {0};
    
    // First time entry
    if (ctx->state == 0) {
        ctx->state = 1;
        clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);
        yield_count = 0;
    }
    
    while (self->state == INFRAX_ASYNC_PENDING) {
        // Check if we've run long enough
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long elapsed_ms = (current_time.tv_sec - ctx->start_time.tv_sec) * 1000 +
                         (current_time.tv_nsec - ctx->start_time.tv_nsec) / 1000000;
                         
        if (elapsed_ms >= TASK_LIFETIME_MS) {
            // Task completed
            ctx->state = 2;
            __atomic_fetch_add(&metrics.completed_tasks, 1, __ATOMIC_SEQ_CST);
            self->state = INFRAX_ASYNC_FULFILLED;
            return;
        }
        
        // Calculate time since last yield
        long yield_interval_ms = 0;
        if (last_yield_time.tv_sec != 0) {
            yield_interval_ms = (current_time.tv_sec - last_yield_time.tv_sec) * 1000 +
                              (current_time.tv_nsec - last_yield_time.tv_nsec) / 1000000;
        }
        
        // Adaptive yield strategy
        yield_count++;
        
        // 更宽松的yield次数限制
        if (yield_count > MAX_YIELD_COUNT) {
            // Force terminate if yielding too much
            self->state = INFRAX_ASYNC_REJECTED;
            __atomic_fetch_add(&metrics.failed_tasks, 1, __ATOMIC_SEQ_CST);
            return;
        }
        
        // 优化的休眠策略
        size_t current_active = metrics.active_tasks;
        if (current_active > TARGET_CONNECTIONS * 0.9) {
            // 非常高负载：每1000次yield休眠一次
            if (yield_count % 1000 == 0) usleep(10);
        } else if (current_active > TARGET_CONNECTIONS * 0.7) {
            // 高负载：每2000次yield休眠一次
            if (yield_count % 2000 == 0) usleep(5);
        } else {
            // 正常负载：每5000次yield休眠一次
            if (yield_count % 5000 == 0) usleep(1);
        }
        
        // Store last yield time
        last_yield_time = current_time;
        
        // Yield control
        self->yield(self);
    }
}

// Get current memory usage
size_t get_memory_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

// Create and start a batch of timer tasks
void create_task_batch(size_t target_tasks) {
    size_t to_create = target_tasks > metrics.active_tasks ? 
                      target_tasks - metrics.active_tasks : 0;
    to_create = to_create > BATCH_SIZE ? BATCH_SIZE : to_create;
    
    for (size_t i = 0; i < to_create; i++) {
        TaskContext* ctx = malloc(sizeof(TaskContext));
        if (!ctx) continue;  // 内存分配失败，跳过此任务
        
        memset(ctx, 0, sizeof(TaskContext));  // 初始化内存
        clock_gettime(CLOCK_MONOTONIC, &ctx->start_time);
        ctx->is_active = 1;
        
        // 创建异步任务
        ctx->async = InfraxAsyncClass.new(long_running_task, ctx);
        if (!ctx->async) {  // 检查任务创建是否成功
            free(ctx);
            continue;
        }
        
        // 增加活跃任务计数
        size_t current_active = __atomic_fetch_add(&metrics.active_tasks, 1, __ATOMIC_SEQ_CST) + 1;
        if (current_active > metrics.peak_active_tasks) {
            metrics.peak_active_tasks = current_active;
        }
        
        // 启动任务 - 修正：直接启动，不重复设置函数和参数
        ctx->async->start(ctx->async, NULL, NULL);
        
        if (ctx->async->state == INFRAX_ASYNC_REJECTED) {
            __atomic_fetch_add(&metrics.failed_tasks, 1, __ATOMIC_SEQ_CST);
            cleanup_task(ctx);
        }
    }
}

// Process active tasks
void process_active_tasks() {
    static int cycle_count = 0;
    cycle_count++;
    
    // 优化主循环的休眠策略
    size_t current_active = metrics.active_tasks;
    if (current_active > TARGET_CONNECTIONS * 0.9) {
        if (cycle_count % 10 == 0) {
            usleep(100);  // 高负载时适度休眠
        }
    } else if (current_active > 0) {
        if (cycle_count % 20 == 0) {
            usleep(50);  // 正常负载时减少休眠
        }
    }
}

// Print current metrics
void print_metrics(time_t elapsed_seconds) {
    metrics.cpu_usage = get_cpu_usage();
    metrics.total_memory = get_memory_usage();
    if (metrics.total_memory > metrics.peak_memory) {
        metrics.peak_memory = metrics.total_memory;
    }
    
    printf("\033[2J\033[H");  // 清屏并移到开头
    printf("=== Test Progress: %ld/%d seconds ===\n", elapsed_seconds, TEST_DURATION_SEC);
    printf("Current Active Tasks: %zu\n", metrics.active_tasks);
    printf("Peak Active Tasks:   %zu\n", metrics.peak_active_tasks);
    printf("Completed Tasks:     %zu\n", metrics.completed_tasks);
    printf("Failed Tasks:        %zu\n", metrics.failed_tasks);
    printf("CPU Usage:           %.1f%%\n", metrics.cpu_usage);
    printf("Current Memory:      %.2f MB\n", metrics.total_memory / 1024.0);
    printf("Peak Memory:         %.2f MB\n", metrics.peak_memory / 1024.0);
    printf("Tasks/sec:           %.2f\n", metrics.completed_tasks / (double)elapsed_seconds);
    printf("----------------------------------------\n");
}

int main() {
    printf("Starting 1K Concurrent Tasks Test...\n");
    printf("Target Connections: %d\n", TARGET_CONNECTIONS);
    printf("Test Duration: %d seconds\n", TEST_DURATION_SEC);
    printf("Task Lifetime: %d ms\n", TASK_LIFETIME_MS);
    printf("----------------------------------------\n");
    sleep(2);  // 给操作者时间阅读参数

    clock_gettime(CLOCK_MONOTONIC, &metrics.start_time);
    time_t start_time = time(NULL);
    time_t current_time;
    time_t last_print_time = 0;

    // Main test loop
    while ((current_time = time(NULL)) - start_time < TEST_DURATION_SEC) {
        create_task_batch(TARGET_CONNECTIONS);
        process_active_tasks();
        
        // 每秒更新一次指标
        if (current_time != last_print_time) {
            print_metrics(current_time - start_time);
            last_print_time = current_time;
        }

        usleep(1000); // 1ms
    }

    // Final metrics
    printf("\nTest Completed!\n");
    print_metrics(TEST_DURATION_SEC);
    
    return 0;
}
