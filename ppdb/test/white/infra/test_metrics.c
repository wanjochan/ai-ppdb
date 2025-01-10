#include "test/test_common.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_metrics.h"
#include "test/test_framework.h"

// Basic functionality test
void test_counter(void) {
    ppdb_metrics_t metrics;
    ppdb_metrics_init(&metrics);

    // 测试初始状态
    ASSERT_EQ(ppdb_metrics_get_throughput(&metrics), 0.0);
    ASSERT_EQ(ppdb_metrics_get_avg_latency(&metrics), 0.0);
    ASSERT_EQ(ppdb_metrics_get_active_threads(&metrics), 0);
    ASSERT_EQ(ppdb_metrics_get_size(&metrics), 0);

    // 测试单个操作
    ppdb_metrics_begin_op(&metrics);
    usleep(1000); // 休眠1ms模拟操作
    ppdb_metrics_end_op(&metrics, 100);

    ASSERT_GT(ppdb_metrics_get_avg_latency(&metrics), 0.0);
    ASSERT_EQ(ppdb_metrics_get_size(&metrics), 100);

    ppdb_metrics_destroy(&metrics);
}

// Performance test
void test_metrics_performance(void) {
    ppdb_metrics_t metrics;
    ppdb_metrics_init(&metrics);
    
    int64_t start = ppdb_time_now();
    for (int i = 0; i < 1000000; i++) {
        ppdb_metrics_begin_op(&metrics);
        ppdb_metrics_end_op(&metrics, 1);
    }
    int64_t end = ppdb_time_now();
    
    double time_spent = (end - start) / 1000000.0;
    double ops_per_sec = 1000000.0 / time_spent;
    
    TEST_ASSERT(ops_per_sec > 100000.0, "Performance below threshold");
    ppdb_metrics_destroy(&metrics);
}

// Boundary conditions test
void test_metrics_boundary(void) {
    ppdb_metrics_t metrics;
    ppdb_metrics_init(&metrics);
    
    // Test maximum values
    ppdb_metrics_begin_op(&metrics);
    ppdb_metrics_end_op(&metrics, SIZE_MAX);
    TEST_ASSERT(ppdb_metrics_get_size(&metrics) == SIZE_MAX, "Max size handling failed");
    
    // Test zero values
    ppdb_metrics_begin_op(&metrics);
    ppdb_metrics_end_op(&metrics, 0);
    TEST_ASSERT(ppdb_metrics_get_avg_latency(&metrics) >= 0.0, "Zero size handling failed");
    
    ppdb_metrics_destroy(&metrics);
}

// Error handling test
void test_metrics_error_handling(void) {
    ppdb_metrics_t metrics;
    
    // Test uninitialized metrics
    TEST_ASSERT(ppdb_metrics_get_throughput(NULL) == 0.0, "Null metrics handling failed");
    TEST_ASSERT(ppdb_metrics_begin_op(NULL) == -1, "Null metrics operation handling failed");
    
    // Test invalid operations
    ppdb_metrics_init(&metrics);
    ppdb_metrics_end_op(&metrics, 100); // End without begin
    TEST_ASSERT(ppdb_metrics_get_avg_latency(&metrics) >= 0.0, "Invalid operation sequence handling failed");
    
    ppdb_metrics_destroy(&metrics);
}

// Stress test with multiple threads
void test_metrics_stress(void) {
    ppdb_metrics_t metrics;
    ppdb_metrics_init(&metrics);
    
    #define NUM_THREADS 8
    #define OPS_PER_THREAD 100000
    
    ppdb_thread_t* threads[NUM_THREADS];
    ppdb_error_t err;

    for (int i = 0; i < NUM_THREADS; i++) {
        err = ppdb_thread_create(&threads[i], concurrent_worker, &metrics);
        TEST_ASSERT(err == PPDB_OK, "Thread creation failed");
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        err = ppdb_thread_join(threads[i]);
        TEST_ASSERT(err == PPDB_OK, "Thread join failed");
    }
    
    TEST_ASSERT(ppdb_metrics_get_size(&metrics) == NUM_THREADS * OPS_PER_THREAD * 10, 
                "Stress test data integrity failed");
    
    ppdb_metrics_destroy(&metrics);
}

// 并发测试线程函数
static void* concurrent_worker(void* arg) {
    ppdb_metrics_t* metrics = (ppdb_metrics_t*)arg;
    
    for (int i = 0; i < 1000; i++) {
        ppdb_metrics_begin_op(metrics);
        ppdb_time_sleep(100); // 休眠0.1ms模拟操作
        ppdb_metrics_end_op(metrics, 10);
    }
    
    return NULL;
}

// 直方图测试
void test_histogram(void) {
    ppdb_metrics_t metrics;
    ppdb_metrics_init(&metrics);

    // 创建4个线程并发写入
    ppdb_thread_t* threads[4];
    ppdb_error_t err;

    for (int i = 0; i < 4; i++) {
        err = ppdb_thread_create(&threads[i], concurrent_worker, &metrics);
        TEST_ASSERT(err == PPDB_OK, "Thread creation failed");
    }

    // 等待所有线程完成
    for (int i = 0; i < 4; i++) {
        err = ppdb_thread_join(threads[i]);
        TEST_ASSERT(err == PPDB_OK, "Thread join failed");
    }

    // 验证结果
    ASSERT_EQ(ppdb_metrics_get_size(&metrics), 40000); // 4 * 1000 * 10
    ASSERT_GT(ppdb_metrics_get_throughput(&metrics), 0.0);

    // 验证延迟分布
    double p50 = ppdb_metrics_get_latency_percentile(&metrics, 50);
    double p99 = ppdb_metrics_get_latency_percentile(&metrics, 99);
    ASSERT_GT(p99, p50); // 99分位延迟应该大于中位数
    
    ppdb_metrics_destroy(&metrics);
}

// 采样器测试
void test_sampler(void) {
    ppdb_metrics_t metrics;
    ppdb_metrics_init(&metrics);

    // 测试固定间隔写入
    for (int i = 0; i < 100; i++) {
        ppdb_metrics_begin_op(&metrics);
        ppdb_time_sleep(10000); // 休眠10ms
        ppdb_metrics_end_op(&metrics, 100);
    }

    // 验证吞吐量(大约应该是100 ops/s)
    double throughput = ppdb_metrics_get_throughput(&metrics);
    ASSERT_GT(throughput, 80.0);  // 允许20%的误差
    ASSERT_LT(throughput, 120.0);

    // 验证平均延迟(大约应该是10ms)
    double avg_latency = ppdb_metrics_get_avg_latency(&metrics);
    ASSERT_GT(avg_latency, 8000.0);  // 8ms
    ASSERT_LT(avg_latency, 12000.0); // 12ms

    // 验证采样率
    double sample_rate = ppdb_metrics_get_sample_rate(&metrics);
    ASSERT_GT(sample_rate, 0.0);
    ASSERT_LE(sample_rate, 1.0);

    ppdb_metrics_destroy(&metrics);
}

int main(void) {
    TEST_INIT("Performance Metrics Test");
    
    // Basic tests
    RUN_TEST(test_counter);
    RUN_TEST(test_histogram);
    RUN_TEST(test_sampler);
    
    // Additional comprehensive tests
    RUN_TEST(test_metrics_performance);
    RUN_TEST(test_metrics_boundary);
    RUN_TEST(test_metrics_error_handling);
    RUN_TEST(test_metrics_stress);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 