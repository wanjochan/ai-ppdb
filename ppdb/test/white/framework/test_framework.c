#include "test/white/framework/test_framework.h"
#include "cosmopolitan.h"

// 全局测试统计
test_stats_t g_test_stats = {0};

void test_framework_init(void) {
    g_test_stats.total_tests = 0;
    g_test_stats.passed_tests = 0;
    g_test_stats.failed_tests = 0;
    g_test_stats.start_time = infra_time_monotonic();
}

void test_framework_cleanup(void) {
    // 暂时不需要清理
}

void test_framework_run_suite(test_suite_t* suite) {
    if (!suite) {
        printf("Error: NULL test suite\n");
        return;
    }

    printf("\nRunning test suite: %s\n", suite->name);
    printf("----------------------------------------\n");

    for (size_t i = 0; i < suite->case_count; i++) {
        test_framework_run_case(&suite->cases[i]);
    }

    printf("----------------------------------------\n");
}

void test_framework_run_case(test_case_t* test_case) {
    if (!test_case) {
        printf("Error: NULL test case\n");
        return;
    }

    printf("Running test case: %s\n", test_case->name);
    g_test_stats.total_tests++;

    size_t failed_before = g_test_stats.failed_tests;
    test_case->func();
    
    if (g_test_stats.failed_tests == failed_before) {
        g_test_stats.passed_tests++;
        printf("  PASS\n");
    } else {
        printf("  FAIL\n");
    }
}

void test_framework_report(void) {
    infra_time_t end_time = infra_time_monotonic();
    double time_spent = (double)(end_time - g_test_stats.start_time) / 1000000.0;  // 转换为秒

    printf("\nTest Summary:\n");
    printf("----------------------------------------\n");
    printf("Total tests:  %zu\n", g_test_stats.total_tests);
    printf("Passed tests: %zu\n", g_test_stats.passed_tests);
    printf("Failed tests: %zu\n", g_test_stats.failed_tests);
    printf("Time spent:   %.2f seconds\n", time_spent);
    printf("----------------------------------------\n");
} 