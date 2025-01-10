#ifndef PPDB_TEST_FRAMEWORK_H
#define PPDB_TEST_FRAMEWORK_H

#include "internal/infra/infra.h"

// 基本断言宏
#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        infra_printf("ASSERT FAILED: %s\n", msg); \
        return 1; \
    }

// 相等性断言
#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        infra_printf("ASSERT FAILED: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        return 1; \
    }

// 大于断言
#define ASSERT_GT(a, b) \
    if ((a) <= (b)) { \
        infra_printf("ASSERT FAILED: %s:%d: %s <= %s\n", __FILE__, __LINE__, #a, #b); \
        return 1; \
    }

// 小于断言
#define ASSERT_LT(a, b) \
    if ((a) >= (b)) { \
        infra_printf("ASSERT FAILED: %s:%d: %s >= %s\n", __FILE__, __LINE__, #a, #b); \
        return 1; \
    }

// 测试运行宏
#define RUN_TEST(test_func) \
    do { \
        infra_printf("Running %s...\n", #test_func); \
        if (test_func() != 0) { \
            infra_printf("Test %s failed\n", #test_func); \
            return 1; \
        } \
        infra_printf("Test %s passed\n", #test_func); \
    } while (0)

// 测试初始化宏
#define TEST_INIT(name) \
    infra_printf("=== Starting Test Suite: %s ===\n\n", name)

// 测试总结宏
#define TEST_SUMMARY() \
    infra_printf("\n=== All Tests Completed Successfully ===\n")

// 测试结果宏
#define TEST_RESULT() (0)

#endif // PPDB_TEST_FRAMEWORK_H 