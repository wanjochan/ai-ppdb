#ifndef PPDB_TEST_MACROS_H
#define PPDB_TEST_MACROS_H

#include <stdio.h>
#include <stdbool.h>

// 测试计数器
extern int g_test_count;
extern int g_test_passed;
extern int g_test_failed;

// 测试用例宏
#define TEST_CASE(test_func) do { \
    printf("Running %s...\n", #test_func); \
    g_test_count++; \
    if (test_func() == 0) { \
        printf("  PASSED\n"); \
        g_test_passed++; \
    } else { \
        printf("  FAILED\n"); \
        g_test_failed++; \
    } \
} while (0)

// 断言宏
#define ASSERT_OK(expr) do { \
    int result = (expr); \
    if (result != PPDB_OK) { \
        fprintf(stderr, "ASSERT_OK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define ASSERT_ERR(expr, expected_err) do { \
    int result = (expr); \
    if (result != (expected_err)) { \
        fprintf(stderr, "ASSERT_ERR failed at %s:%d: %s (expected %d, got %d)\n", \
                __FILE__, __LINE__, #expr, expected_err, result); \
        return 1; \
    } \
} while (0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "ASSERT_TRUE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define ASSERT_FALSE(expr) do { \
    if (expr) { \
        fprintf(stderr, "ASSERT_FALSE failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define ASSERT_EQ(expr1, expr2) do { \
    if ((expr1) != (expr2)) { \
        fprintf(stderr, "ASSERT_EQ failed at %s:%d: %s != %s\n", \
                __FILE__, __LINE__, #expr1, #expr2); \
        return 1; \
    } \
} while (0)

#define ASSERT_GT(expr1, expr2) do { \
    if (!((expr1) > (expr2))) { \
        fprintf(stderr, "ASSERT_GT failed at %s:%d: %s <= %s\n", \
                __FILE__, __LINE__, #expr1, #expr2); \
        return 1; \
    } \
} while (0)

#define ASSERT_NOT_NULL(expr) do { \
    if ((expr) == NULL) { \
        fprintf(stderr, "ASSERT_NOT_NULL failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#endif // PPDB_TEST_MACROS_H 