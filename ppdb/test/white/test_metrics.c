#include <cosmopolitan.h>
#include "test.h"
#include "internal/metrics.h"
#include "ppdb/logger.h"

// 基本功能测试
void test_metrics_basic() {
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
    TEST_OK();
}

// 并发测试
static void* concurrent_worker(void* arg) {
    ppdb_metrics_t* metrics = (ppdb_metrics_t*)arg;
    
    for (int i = 0; i < 1000; i++) {
        ppdb_metrics_begin_op(metrics);
        usleep(100); // 休眠0.1ms模拟操作
        ppdb_metrics_end_op(metrics, 10);
    }
    
    return NULL;
}

void test_metrics_concurrent() {
    ppdb_metrics_t metrics;
    ppdb_metrics_init(&metrics);

    // 创建4个线程并发写入
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, concurrent_worker, &metrics);
    }

    // 等待所有线程完成
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    // 验证结果
    ASSERT_EQ(ppdb_metrics_get_size(&metrics), 40000); // 4 * 1000 * 10
    ASSERT_GT(ppdb_metrics_get_throughput(&metrics), 0.0);
    
    ppdb_metrics_destroy(&metrics);
    TEST_OK();
}

// 性能指标准确性测试
void test_metrics_accuracy() {
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

    ppdb_metrics_destroy(&metrics);
    TEST_OK();
}

// memtable性能监控测试
void test_memtable_metrics() {
    ppdb_memtable_t* table;
    ASSERT_EQ(ppdb_memtable_create(1024*1024, &table), PPDB_OK);

    // 获取性能指标
    ppdb_metrics_t* metrics = ppdb_memtable_get_metrics(table);
    ASSERT_NOT_NULL(metrics);

    // 测试写入操作的性能统计
    uint8_t key[8] = {0};
    uint8_t value[100] = {0};
    
    for (int i = 0; i < 1000; i++) {
        *(uint64_t*)key = i;
        ASSERT_EQ(ppdb_memtable_put(table, key, sizeof(key), value, sizeof(value)), PPDB_OK);
    }

    // 验证性能指标
    ASSERT_GT(ppdb_metrics_get_throughput(metrics), 0.0);
    ASSERT_GT(ppdb_metrics_get_size(metrics), 0);
    ASSERT_EQ(ppdb_metrics_get_active_threads(metrics), 0); // 所有操作已完成

    ppdb_memtable_destroy(table);
    TEST_OK();
}
