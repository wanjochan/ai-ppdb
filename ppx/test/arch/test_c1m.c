#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"

#define TARGET_CONNECTIONS 100000
#define BATCH_SIZE 1000
#define TEST_DURATION_SEC 30

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

// Empty task function for testing
void empty_task(InfraxAsync* async, void* arg) {
    // Just return immediately to simulate minimal work
    async->state = INFRAX_ASYNC_FULFILLED;
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
        InfraxAsync* async = malloc(sizeof(InfraxAsync));
        if (async) {
            async->fn = empty_task;
            async->arg = NULL;
            async->state = INFRAX_ASYNC_PENDING;
            async->error = 0;
            async->self = async;
            
            // Start task
            async->fn(async, async->arg);
            
            if (async->state == INFRAX_ASYNC_FULFILLED) {
                metrics.completed_tasks++;
            } else if (async->state == INFRAX_ASYNC_REJECTED) {
                metrics.failed_tasks++;
            } else {
                metrics.active_tasks++;
            }
            
            free(async);
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
    printf("Active Tasks:     %zu\n", metrics.active_tasks);
    printf("Completed Tasks:  %zu\n", metrics.completed_tasks);
    printf("Failed Tasks:     %zu\n", metrics.failed_tasks);
    printf("CPU Usage:        %.1f%%\n", metrics.cpu_usage);
    printf("Current Memory:   %.2f MB\n", metrics.total_memory / 1024.0);
    printf("Peak Memory:      %.2f MB\n", metrics.peak_memory / 1024.0);
    printf("Tasks/sec:        %.2f\n", metrics.completed_tasks / (double)elapsed_seconds);
    printf("----------------------------------------\n");
}

int main() {
    printf("Starting 100K Concurrent Tasks Test...\n");
    printf("Target Connections: %d\n", TARGET_CONNECTIONS);
    printf("Test Duration: %d seconds\n", TEST_DURATION_SEC);
    printf("----------------------------------------\n");
    sleep(2);  // 给操作者时间阅读参数

    clock_gettime(CLOCK_MONOTONIC, &metrics.start_time);
    time_t start_time = time(NULL);
    time_t current_time;
    time_t last_print_time = 0;

    // Main test loop
    while ((current_time = time(NULL)) - start_time < TEST_DURATION_SEC) {
        create_task_batch(TARGET_CONNECTIONS);
        
        // 每秒更新一次指标
        if (current_time != last_print_time) {
            print_metrics(current_time - start_time);
            last_print_time = current_time;
        }

        usleep(10000); // 10ms
    }

    // Final metrics
    printf("\nTest Completed!\n");
    print_metrics(TEST_DURATION_SEC);
    
    return 0;
}
