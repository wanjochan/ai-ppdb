#ifndef PPDB_TEST_FRAMEWORK_H
#define PPDB_TEST_FRAMEWORK_H

#include "internal/infra/infra_core.h"

// 测试统计信息
typedef struct test_stats {
    size_t total_tests;
    size_t passed_tests;
    size_t failed_tests;
    infra_time_t start_time;
} test_stats_t;

// 全局测试统计
extern test_stats_t g_test_stats;

// 测试函数类型
typedef void (*test_func_t)(void);

// 测试用例结构
typedef struct {
    const char* name;
    test_func_t func;
} test_case_t;

// 测试套件结构
typedef struct {
    const char* name;
    test_case_t* cases;
    size_t case_count;
} test_suite_t;

// 测试框架主要宏
#define TEST_BEGIN(name) \
    do { \
        printf("\nRunning test suite: %s\n", name); \
        test_framework_init(); \
    } while (0)

#define TEST_END() \
    do { \
        test_framework_report(); \
        test_framework_cleanup(); \
        return g_test_stats.failed_tests ? 1 : 0; \
    } while (0)

#define RUN_TEST(test_func) \
    do { \
        printf("Running test: %s\n", #test_func); \
        test_case_t test_case = {#test_func, test_func}; \
        test_framework_run_case(&test_case); \
    } while (0)

// 测试框架函数
void test_framework_init(void);
void test_framework_cleanup(void);
void test_framework_run_suite(test_suite_t* suite);
void test_framework_run_case(test_case_t* test_case);
void test_framework_report(void);

// 断言宏
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: %s\n", #condition); \
            g_test_stats.failed_tests++; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_MSG(condition, format, ...) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: " format "\n", ##__VA_ARGS__); \
            g_test_stats.failed_tests++; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("Assertion failed: expected %d, got %d\n", (int)(expected), (int)(actual)); \
            g_test_stats.failed_tests++; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_STRING_EQUAL(expected, actual) \
    do { \
        if (infra_strcmp((expected), (actual)) != 0) { \
            printf("Assertion failed: expected \"%s\", got \"%s\"\n", (expected), (actual)); \
            g_test_stats.failed_tests++; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf("Assertion failed: expected NULL, got %p\n", (void*)(ptr)); \
            g_test_stats.failed_tests++; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("Assertion failed: expected non-NULL\n"); \
            g_test_stats.failed_tests++; \
            return; \
        } \
    } while (0)

#endif // PPDB_TEST_FRAMEWORK_H