#include <cosmopolitan.h>
#include "internal/base.h"
#include "test_common.h"

// 测试配置
#define NUM_THREADS 2
#define OPS_PER_THREAD 1000
#define VALUE_SIZE 100

// 前向声明
struct test_context_s;

// 线程参数
typedef struct thread_args_s {
    struct test_context_s* ctx;
    int thread_id;
    uint64_t total_time_us;
    uint64_t contention_count;
} thread_args_t;

// 共享数据结构
typedef struct test_context_s {
    ppdb_base_t* base;
    ppdb_base_mutex_t* mutex;
    ppdb_base_spinlock_t* spinlock;
    atomic_size_t counter;
    char* shared_buffer;
    thread_args_t thread_args[NUM_THREADS];
    ppdb_base_thread_t* threads[NUM_THREADS];
} test_context_t;

// 获取当前时间（微秒）
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

// 使用互斥锁的线程函数
static void* mutex_thread_func(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    test_context_t* ctx = args->ctx;
    uint64_t start_time, end_time;
    
    printf("Thread %d started\n", args->thread_id);
    fflush(stdout);
    
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        start_time = get_time_us();
        
        // 加锁
        ppdb_base_mutex_lock(ctx->mutex);
        
        end_time = get_time_us();
        args->total_time_us += (end_time - start_time);
        
        // 临界区操作
        atomic_fetch_add(&ctx->counter, 1);
        memset(ctx->shared_buffer, (char)i, VALUE_SIZE);
        
        // 解锁
        ppdb_base_mutex_unlock(ctx->mutex);
        
        // 模拟其他工作
        if (i % 100 == 0) {
            usleep(1);
            printf("Thread %d completed %d operations\n", args->thread_id, i);
            fflush(stdout);
        }
    }
    
    printf("Thread %d completed all operations\n", args->thread_id);
    fflush(stdout);
    return NULL;
}

// 使用自旋锁的线程函数
static void* spinlock_thread_func(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    test_context_t* ctx = args->ctx;
    uint64_t start_time, end_time;
    
    printf("Thread %d started\n", args->thread_id);
    fflush(stdout);
    
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        start_time = get_time_us();
        
        // 加锁
        ppdb_base_spinlock_lock(ctx->spinlock);
        
        end_time = get_time_us();
        args->total_time_us += (end_time - start_time);
        
        // 临界区操作
        atomic_fetch_add(&ctx->counter, 1);
        memset(ctx->shared_buffer, (char)i, VALUE_SIZE);
        
        // 解锁
        ppdb_base_spinlock_unlock(ctx->spinlock);
        
        // 模拟其他工作
        if (i % 100 == 0) {
            usleep(1);
            printf("Thread %d completed %d operations\n", args->thread_id, i);
            fflush(stdout);
        }
    }
    
    printf("Thread %d completed all operations\n", args->thread_id);
    fflush(stdout);
    return NULL;
}

// 测试互斥锁性能
static void test_mutex_performance(void) {
    printf("Running mutex performance test...\n");
    fflush(stdout);
    
    // 初始化测试上下文
    test_context_t* ctx = calloc(1, sizeof(test_context_t));
    assert(ctx != NULL);
    
    // 初始化 base
    printf("Initializing base...\n");
    fflush(stdout);
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = NUM_THREADS,
        .thread_safe = true
    };
    assert(ppdb_base_init(&ctx->base, &base_config) == PPDB_OK);
    
    // 初始化互斥锁和共享内存
    printf("Creating mutex...\n");
    fflush(stdout);
    assert(ppdb_base_mutex_create(&ctx->mutex) == PPDB_OK);
    ctx->shared_buffer = malloc(VALUE_SIZE);
    assert(ctx->shared_buffer != NULL);
    atomic_store(&ctx->counter, 0);
    
    // 启动线程
    printf("Starting threads...\n");
    fflush(stdout);
    for (int i = 0; i < NUM_THREADS; i++) {
        ctx->thread_args[i].ctx = ctx;
        ctx->thread_args[i].thread_id = i;
        ctx->thread_args[i].total_time_us = 0;
        ctx->thread_args[i].contention_count = 0;
        printf("Creating thread %d...\n", i);
        fflush(stdout);
        assert(ppdb_base_thread_create(&ctx->threads[i], mutex_thread_func, &ctx->thread_args[i]) == PPDB_OK);
        printf("Thread %d created\n", i);
        fflush(stdout);
    }
    
    // 等待线程完成
    printf("Waiting for threads to complete...\n");
    fflush(stdout);
    uint64_t total_time_us = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("Joining thread %d...\n", i);
        fflush(stdout);
        ppdb_base_thread_join(ctx->threads[i], NULL);
        printf("Thread %d joined\n", i);
        fflush(stdout);
        ppdb_base_thread_destroy(ctx->threads[i]);
        total_time_us += ctx->thread_args[i].total_time_us;
    }
    
    // 输出统计信息
    size_t total_ops = NUM_THREADS * OPS_PER_THREAD;
    double avg_latency_us = (double)total_time_us / total_ops;
    double ops_per_sec = (double)total_ops / (total_time_us / 1000000.0);
    
    printf("Mutex Performance Results:\n");
    printf("  Total Operations: %zu\n", total_ops);
    printf("  Total Time: %.2f seconds\n", total_time_us / 1000000.0);
    printf("  Average Latency: %.2f microseconds\n", avg_latency_us);
    printf("  Operations/Second: %.2f\n", ops_per_sec);
    printf("  Final Counter: %zu\n", atomic_load(&ctx->counter));
    fflush(stdout);
    
    // 清理资源
    printf("Cleaning up mutex test resources...\n");
    fflush(stdout);
    free(ctx->shared_buffer);
    ppdb_base_mutex_destroy(ctx->mutex);
    ppdb_base_destroy(ctx->base);
    free(ctx);
}

// 测试自旋锁性能
static void test_spinlock_performance(void) {
    printf("\nRunning spinlock performance test...\n");
    fflush(stdout);
    
    // 初始化测试上下文
    test_context_t* ctx = calloc(1, sizeof(test_context_t));
    assert(ctx != NULL);
    
    // 初始化 base
    printf("Initializing base...\n");
    fflush(stdout);
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = NUM_THREADS,
        .thread_safe = true
    };
    assert(ppdb_base_init(&ctx->base, &base_config) == PPDB_OK);
    
    // 初始化自旋锁和共享内存
    printf("Creating spinlock...\n");
    fflush(stdout);
    assert(ppdb_base_spinlock_create(&ctx->spinlock) == PPDB_OK);
    ctx->shared_buffer = malloc(VALUE_SIZE);
    assert(ctx->shared_buffer != NULL);
    atomic_store(&ctx->counter, 0);
    
    // 启动线程
    printf("Starting threads...\n");
    fflush(stdout);
    for (int i = 0; i < NUM_THREADS; i++) {
        ctx->thread_args[i].ctx = ctx;
        ctx->thread_args[i].thread_id = i;
        ctx->thread_args[i].total_time_us = 0;
        ctx->thread_args[i].contention_count = 0;
        printf("Creating thread %d...\n", i);
        fflush(stdout);
        assert(ppdb_base_thread_create(&ctx->threads[i], spinlock_thread_func, &ctx->thread_args[i]) == PPDB_OK);
        printf("Thread %d created\n", i);
        fflush(stdout);
    }
    
    // 等待线程完成
    printf("Waiting for threads to complete...\n");
    fflush(stdout);
    uint64_t total_time_us = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("Joining thread %d...\n", i);
        fflush(stdout);
        ppdb_base_thread_join(ctx->threads[i], NULL);
        printf("Thread %d joined\n", i);
        fflush(stdout);
        ppdb_base_thread_destroy(ctx->threads[i]);
        total_time_us += ctx->thread_args[i].total_time_us;
    }
    
    // 输出统计信息
    size_t total_ops = NUM_THREADS * OPS_PER_THREAD;
    double avg_latency_us = (double)total_time_us / total_ops;
    double ops_per_sec = (double)total_ops / (total_time_us / 1000000.0);
    
    printf("Spinlock Performance Results:\n");
    printf("  Total Operations: %zu\n", total_ops);
    printf("  Total Time: %.2f seconds\n", total_time_us / 1000000.0);
    printf("  Average Latency: %.2f microseconds\n", avg_latency_us);
    printf("  Operations/Second: %.2f\n", ops_per_sec);
    printf("  Final Counter: %zu\n", atomic_load(&ctx->counter));
    fflush(stdout);
    
    // 清理资源
    printf("Cleaning up spinlock test resources...\n");
    fflush(stdout);
    free(ctx->shared_buffer);
    ppdb_base_spinlock_destroy(ctx->spinlock);
    ppdb_base_destroy(ctx->base);
    free(ctx);
}

int main(void) {
    printf("Running Synchronization Performance Tests\n");
    fflush(stdout);
    
    // 运行互斥锁测试
    test_mutex_performance();
    
    // 运行自旋锁测试
    test_spinlock_performance();
    
    printf("\nAll performance tests completed\n");
    fflush(stdout);
    return 0;
} 