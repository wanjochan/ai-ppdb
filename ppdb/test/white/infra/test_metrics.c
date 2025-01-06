#include <cosmopolitan.h>
#include "test_framework.h"
#include "kvstore/internal/metrics.h"
#include "../../src/internal/base.h"

// 计数器测试
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

// 并发测试线程函数
static void* concurrent_worker(void* arg) {
    ppdb_metrics_t* metrics = (ppdb_metrics_t*)arg;
    
    for (int i = 0; i < 1000; i++) {
        ppdb_metrics_begin_op(metrics);
        usleep(100); // 休眠0.1ms模拟操作
        ppdb_metrics_end_op(metrics, 10);
    }
    
    return NULL;
}

// 直方图测试
void test_histogram(void) {
    ppdb_metrics_t metrics;
    ppdb_metrics_init(&metrics);

    // 创建4个线程并发写入
    ppdb_base_thread_t* threads[4];
    for (int i = 0; i < 4; i++) {
        threads[i] = ppdb_base_thread_create(concurrent_worker, &metrics);
    }

    // 等待所有线程完成
    for (int i = 0; i < 4; i++) {
        ppdb_base_thread_join(threads[i]);
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
        usleep(10000); // 休眠10ms
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
    
    RUN_TEST(test_counter);
    RUN_TEST(test_histogram);
    RUN_TEST(test_sampler);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 