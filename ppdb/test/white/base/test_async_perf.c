#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_framework.h"

// 全局测试数据
static ppdb_base_t* g_base = NULL;

// 性能统计数据
typedef struct {
    int64_t total_ops;
    int64_t total_time_ns;
    int64_t min_time_ns;
    int64_t max_time_ns;
    double avg_time_ns;
    double ops_per_sec;
} perf_stats_t;

// 测试初始化
static int test_setup(void) {
    printf("\n=== Setting up async performance test environment ===\n");
    
    // 初始化 base 配置
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 100,  // 100MB
        .thread_pool_size = 8,
        .thread_safe = true,
        .enable_logging = true,
        .log_level = PPDB_LOG_DEBUG
    };
    
    // 初始化 base 层
    ASSERT_OK(ppdb_base_init(&g_base, &base_config));
    
    printf("Test environment setup completed\n");
    return 0;
}

// 测试清理
static int test_teardown(void) {
    printf("\n=== Cleaning up async performance test environment ===\n");
    
    if (g_base) {
        ppdb_base_destroy(g_base);
        g_base = NULL;
    }
    
    printf("Test environment cleanup completed\n");
    return 0;
}

// 性能统计初始化
static void init_perf_stats(perf_stats_t* stats) {
    stats->total_ops = 0;
    stats->total_time_ns = 0;
    stats->min_time_ns = INT64_MAX;
    stats->max_time_ns = 0;
    stats->avg_time_ns = 0.0;
    stats->ops_per_sec = 0.0;
}

// 更新性能统计
static void update_perf_stats(perf_stats_t* stats, int64_t op_time_ns) {
    stats->total_ops++;
    stats->total_time_ns += op_time_ns;
    stats->min_time_ns = MIN(stats->min_time_ns, op_time_ns);
    stats->max_time_ns = MAX(stats->max_time_ns, op_time_ns);
    stats->avg_time_ns = (double)stats->total_time_ns / stats->total_ops;
    stats->ops_per_sec = 1e9 * stats->total_ops / stats->total_time_ns;
}

// 打印性能统计
static void print_perf_stats(const char* test_name, perf_stats_t* stats) {
    printf("\n=== Performance Statistics for %s ===\n", test_name);
    printf("Total Operations: %ld\n", stats->total_ops);
    printf("Total Time: %.2f ms\n", stats->total_time_ns / 1e6);
    printf("Min Time: %.2f us\n", stats->min_time_ns / 1e3);
    printf("Max Time: %.2f us\n", stats->max_time_ns / 1e3);
    printf("Avg Time: %.2f us\n", stats->avg_time_ns / 1e3);
    printf("Throughput: %.2f ops/sec\n", stats->ops_per_sec);
    printf("=====================================\n");
}

// 异步任务数据
typedef struct {
    int task_id;
    int64_t iterations;
    perf_stats_t stats;
} async_task_data_t;

// 异步任务回调
static void async_task_complete(ppdb_error_t error, void* result, void* user_data) {
    async_task_data_t* data = (async_task_data_t*)user_data;
    int64_t end_time = ppdb_base_get_time_ns();
    
    if (error == PPDB_OK) {
        update_perf_stats(&data->stats, end_time - (int64_t)result);
    }
}

// 异步任务函数
static void async_task_func(void* arg) {
    async_task_data_t* data = (async_task_data_t*)arg;
    int64_t start_time = ppdb_base_get_time_ns();
    
    // 模拟异步操作
    ppdb_base_sleep_us(1);
    
    // 完成回调
    async_task_complete(PPDB_OK, (void*)start_time, data);
}

// 异步性能测试
static int test_async_performance(void) {
    printf("\n=== Running async performance test ===\n");
    
    const int num_tasks = 4;
    const int64_t iterations_per_task = 10000;
    
    // 创建任务数据
    async_task_data_t* task_data = ppdb_base_malloc(sizeof(async_task_data_t) * num_tasks);
    
    // 初始化任务
    for (int i = 0; i < num_tasks; i++) {
        task_data[i].task_id = i;
        task_data[i].iterations = iterations_per_task;
        init_perf_stats(&task_data[i].stats);
    }
    
    // 提交异步任务
    for (int i = 0; i < num_tasks; i++) {
        for (int64_t j = 0; j < iterations_per_task; j++) {
            ASSERT_OK(ppdb_base_async_submit(g_base, async_task_func, &task_data[i]));
        }
    }
    
    // 等待所有任务完成
    ppdb_base_async_wait_all(g_base);
    
    // 合并统计数据
    perf_stats_t total_stats;
    init_perf_stats(&total_stats);
    
    for (int i = 0; i < num_tasks; i++) {
        total_stats.total_ops += task_data[i].stats.total_ops;
        total_stats.total_time_ns += task_data[i].stats.total_time_ns;
        total_stats.min_time_ns = MIN(total_stats.min_time_ns, task_data[i].stats.min_time_ns);
        total_stats.max_time_ns = MAX(total_stats.max_time_ns, task_data[i].stats.max_time_ns);
    }
    
    total_stats.avg_time_ns = (double)total_stats.total_time_ns / total_stats.total_ops;
    total_stats.ops_per_sec = 1e9 * total_stats.total_ops / total_stats.total_time_ns;
    
    // 打印结果
    print_perf_stats("Async Performance Test", &total_stats);
    
    // 清理
    ppdb_base_free(task_data);
    
    return 0;
}

// 测试入口
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    PPDB_TEST_BEGIN();
    
    PPDB_TEST_RUN(test_setup);
    PPDB_TEST_RUN(test_async_performance);
    PPDB_TEST_RUN(test_teardown);
    
    PPDB_TEST_END();
    
    return 0;
} 