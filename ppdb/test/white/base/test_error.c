#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_framework.h"

// 全局测试数据
static ppdb_base_t* g_base = NULL;

// 错误统计数据
typedef struct {
    int64_t total_errors;
    int64_t error_by_type[PPDB_ERR_MAX];
    const char* last_error_msg;
} error_stats_t;

// 测试初始化
static int test_setup(void) {
    printf("\n=== Setting up error test environment ===\n");
    
    // 初始化 base 配置
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .thread_pool_size = 4,
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
    printf("\n=== Cleaning up error test environment ===\n");
    
    if (g_base) {
        ppdb_base_destroy(g_base);
        g_base = NULL;
    }
    
    printf("Test environment cleanup completed\n");
    return 0;
}

// 初始化错误统计
static void init_error_stats(error_stats_t* stats) {
    stats->total_errors = 0;
    memset(stats->error_by_type, 0, sizeof(stats->error_by_type));
    stats->last_error_msg = NULL;
}

// 更新错误统计
static void update_error_stats(error_stats_t* stats, ppdb_error_t error, const char* msg) {
    stats->total_errors++;
    stats->error_by_type[error]++;
    stats->last_error_msg = msg;
}

// 打印错误统计
static void print_error_stats(const char* test_name, error_stats_t* stats) {
    printf("\n=== Error Statistics for %s ===\n", test_name);
    printf("Total Errors: %ld\n", stats->total_errors);
    printf("Error Distribution:\n");
    for (int i = 0; i < PPDB_ERR_MAX; i++) {
        if (stats->error_by_type[i] > 0) {
            printf("  Error %d: %ld occurrences\n", i, stats->error_by_type[i]);
        }
    }
    if (stats->last_error_msg) {
        printf("Last Error Message: %s\n", stats->last_error_msg);
    }
    printf("=====================================\n");
}

// 基本错误处理测试
static int test_error_basic(void) {
    printf("\n=== Running basic error tests ===\n");
    
    error_stats_t stats;
    init_error_stats(&stats);
    
    // 测试错误码设置和获取
    ppdb_error_t err = PPDB_ERR_NULL_POINTER;
    ppdb_base_set_error(g_base, err, "Null pointer error");
    update_error_stats(&stats, err, "Null pointer error");
    
    ASSERT_EQ(ppdb_base_get_error(g_base), err);
    
    // 测试错误消息
    const char* msg = ppdb_base_get_error_message(g_base);
    ASSERT_NOT_NULL(msg);
    ASSERT_EQ(strcmp(msg, "Null pointer error"), 0);
    
    // 测试错误清除
    ppdb_base_clear_error(g_base);
    ASSERT_EQ(ppdb_base_get_error(g_base), PPDB_OK);
    
    print_error_stats("Basic Error Test", &stats);
    return 0;
}

// 错误传播测试
static int test_error_propagation(void) {
    printf("\n=== Running error propagation tests ===\n");
    
    error_stats_t stats;
    init_error_stats(&stats);
    
    // 模拟错误传播链
    ppdb_error_t err1 = PPDB_ERR_IO;
    ppdb_base_set_error(g_base, err1, "IO error occurred");
    update_error_stats(&stats, err1, "IO error occurred");
    
    ppdb_error_t err2 = PPDB_ERR_TRANSACTION;
    ppdb_base_set_error(g_base, err2, "Transaction failed due to IO error");
    update_error_stats(&stats, err2, "Transaction failed due to IO error");
    
    // 验证最后的错误
    ASSERT_EQ(ppdb_base_get_error(g_base), err2);
    
    const char* msg = ppdb_base_get_error_message(g_base);
    ASSERT_NOT_NULL(msg);
    ASSERT_EQ(strcmp(msg, "Transaction failed due to IO error"), 0);
    
    print_error_stats("Error Propagation Test", &stats);
    return 0;
}

// 错误边界测试
static int test_error_boundary(void) {
    printf("\n=== Running error boundary tests ===\n");
    
    error_stats_t stats;
    init_error_stats(&stats);
    
    // 测试无效错误码
    ppdb_error_t invalid_err = PPDB_ERR_MAX + 1;
    ppdb_base_set_error(g_base, invalid_err, "Invalid error code");
    update_error_stats(&stats, invalid_err, "Invalid error code");
    
    // 测试空错误消息
    ppdb_error_t err = PPDB_ERR_MEMORY;
    ppdb_base_set_error(g_base, err, NULL);
    update_error_stats(&stats, err, "NULL");
    
    ASSERT_EQ(ppdb_base_get_error(g_base), err);
    ASSERT_NOT_NULL(ppdb_base_get_error_message(g_base));
    
    // 测试长错误消息
    char long_msg[1024];
    memset(long_msg, 'A', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';
    
    ppdb_base_set_error(g_base, err, long_msg);
    update_error_stats(&stats, err, "Long error message");
    
    const char* msg = ppdb_base_get_error_message(g_base);
    ASSERT_NOT_NULL(msg);
    ASSERT_EQ(strlen(msg), strlen(long_msg));
    
    print_error_stats("Error Boundary Test", &stats);
    return 0;
}

// 并发错误处理测试
typedef struct {
    ppdb_base_t* base;
    int thread_id;
    int64_t iterations;
    error_stats_t stats;
} error_thread_data_t;

static void* error_thread(void* arg) {
    error_thread_data_t* data = (error_thread_data_t*)arg;
    
    for (int64_t i = 0; i < data->iterations; i++) {
        ppdb_error_t err = PPDB_ERR_IO + (i % 3);  // 循环使用不同的错误码
        char msg[64];
        snprintf(msg, sizeof(msg), "Thread %d error %ld", data->thread_id, i);
        
        ppdb_base_set_error(data->base, err, msg);
        update_error_stats(&data->stats, err, msg);
        
        // 模拟一些工作
        ppdb_base_sleep_us(1);
        
        ppdb_base_clear_error(data->base);
    }
    
    return NULL;
}

static int test_error_concurrent(void) {
    printf("\n=== Running concurrent error tests ===\n");
    
    const int num_threads = 4;
    const int64_t iterations_per_thread = 1000;
    
    // 创建线程数据
    error_thread_data_t* thread_data = ppdb_base_malloc(sizeof(error_thread_data_t) * num_threads);
    ppdb_base_thread_t* threads = ppdb_base_malloc(sizeof(ppdb_base_thread_t) * num_threads);
    
    // 启动线程
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].base = g_base;
        thread_data[i].thread_id = i;
        thread_data[i].iterations = iterations_per_thread;
        init_error_stats(&thread_data[i].stats);
        
        ASSERT_OK(ppdb_base_thread_create(&threads[i], error_thread, &thread_data[i]));
    }
    
    // 等待线程完成
    for (int i = 0; i < num_threads; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i], NULL));
    }
    
    // 合并统计数据
    error_stats_t total_stats;
    init_error_stats(&total_stats);
    
    for (int i = 0; i < num_threads; i++) {
        total_stats.total_errors += thread_data[i].stats.total_errors;
        for (int j = 0; j < PPDB_ERR_MAX; j++) {
            total_stats.error_by_type[j] += thread_data[i].stats.error_by_type[j];
        }
    }
    
    // 打印结果
    print_error_stats("Concurrent Error Test", &total_stats);
    
    // 清理
    ppdb_base_free(thread_data);
    ppdb_base_free(threads);
    
    return 0;
}

int main(void) {
    if (test_setup() != 0) {
        printf("Test setup failed\n");
        return 1;
    }
    
    TEST_CASE(test_error_basic);
    TEST_CASE(test_error_propagation);
    TEST_CASE(test_error_boundary);
    TEST_CASE(test_error_concurrent);
    
    if (test_teardown() != 0) {
        printf("Test teardown failed\n");
        return 1;
    }
    
    printf("\nTest summary:\n");
    printf("  Total: %d\n", g_test_count);
    printf("  Passed: %d\n", g_test_passed);
    printf("  Failed: %d\n", g_test_failed);
    
    return g_test_failed > 0 ? 1 : 0;
}