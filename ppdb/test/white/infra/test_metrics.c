#include "test_common.h"
#include "internal/infra/infra.h"
#include "test_framework.h"

// 基本功能测试
static int test_metrics_basic(void) {
    infra_stats_t stats;
    infra_stats_init(&stats);

    // 测试初始状态
    TEST_ASSERT(stats.total_operations == 0);
    TEST_ASSERT(stats.successful_operations == 0);
    TEST_ASSERT(stats.failed_operations == 0);
    TEST_ASSERT(stats.total_bytes == 0);
    TEST_ASSERT(stats.min_latency_us == (uint64_t)-1);
    TEST_ASSERT(stats.max_latency_us == 0);
    TEST_ASSERT(stats.avg_latency_us == 0);

    // 测试单个操作
    infra_stats_update(&stats, true, 1000, 100, INFRA_OK);
    TEST_ASSERT(stats.total_operations == 1);
    TEST_ASSERT(stats.successful_operations == 1);
    TEST_ASSERT(stats.total_bytes == 100);
    TEST_ASSERT(stats.min_latency_us == 1000);
    TEST_ASSERT(stats.max_latency_us == 1000);
    TEST_ASSERT(stats.avg_latency_us == 1000);

    return 0;
}

// 性能测试
static int test_metrics_performance(void) {
    infra_stats_t stats;
    infra_stats_init(&stats);
    
    infra_time_t start = infra_time_monotonic();
    for (int i = 0; i < 1000000; i++) {
        infra_stats_update(&stats, true, 1, 1, INFRA_OK);
    }
    infra_time_t end = infra_time_monotonic();
    
    double time_spent = (double)(end - start) / 1000000.0;  // Convert to seconds
    TEST_ASSERT(time_spent < 30.0);  // 性能测试应在30秒内完成
    
    return 0;
}

// 边界条件测试
static int test_metrics_boundary(void) {
    infra_stats_t stats;
    infra_stats_init(&stats);
    
    // 测试最大值
    infra_stats_update(&stats, true, UINT64_MAX, SIZE_MAX, INFRA_OK);
    TEST_ASSERT(stats.total_bytes == SIZE_MAX);
    TEST_ASSERT(stats.max_latency_us == UINT64_MAX);
    
    // 测试零值
    infra_stats_update(&stats, true, 0, 0, INFRA_OK);
    TEST_ASSERT(stats.min_latency_us == 0);
    
    return 0;
}

// 错误处理测试
static int test_metrics_error_handling(void) {
    infra_stats_t stats;
    infra_stats_init(&stats);
    
    // 测试失败操作
    infra_stats_update(&stats, false, 1000, 100, INFRA_ERROR_MEMORY);
    TEST_ASSERT(stats.failed_operations == 1);
    TEST_ASSERT(stats.last_error == INFRA_ERROR_MEMORY);
    TEST_ASSERT(stats.last_error_time > 0);
    
    return 0;
}

// 合并测试
static int test_metrics_merge(void) {
    infra_stats_t stats1, stats2;
    infra_stats_init(&stats1);
    infra_stats_init(&stats2);
    
    // 更新第一个统计对象
    infra_stats_update(&stats1, true, 1000, 100, INFRA_OK);
    infra_stats_update(&stats1, false, 2000, 200, INFRA_ERROR_MEMORY);
    
    // 更新第二个统计对象
    infra_stats_update(&stats2, true, 3000, 300, INFRA_OK);
    infra_stats_update(&stats2, true, 4000, 400, INFRA_OK);
    
    // 合并统计
    infra_stats_merge(&stats1, &stats2);
    
    TEST_ASSERT(stats1.total_operations == 4);
    TEST_ASSERT(stats1.successful_operations == 3);
    TEST_ASSERT(stats1.failed_operations == 1);
    TEST_ASSERT(stats1.total_bytes == 1000);
    TEST_ASSERT(stats1.min_latency_us == 1000);
    TEST_ASSERT(stats1.max_latency_us == 4000);
    
    return 0;
}

int main(void) {
    // 初始化infra系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    TEST_INIT();
    
    TEST_RUN(test_metrics_basic);
    TEST_RUN(test_metrics_performance);
    TEST_RUN(test_metrics_boundary);
    TEST_RUN(test_metrics_error_handling);
    TEST_RUN(test_metrics_merge);
    
    TEST_CLEANUP();
    
    // 清理infra系统
    infra_cleanup();
    return 0;
} 