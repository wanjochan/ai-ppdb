#ifndef PPDB_TEST_MACROS_H
#define PPDB_TEST_MACROS_H

#include "test_framework.h"

// 测试初始化宏
#define TEST_INIT() do { \
    test_framework_init(); \
    printf("\nStarting test suite...\n"); \
} while(0)

// 测试清理宏
#define TEST_CLEANUP() do { \
    test_framework_cleanup(); \
} while(0)

// 测试总结宏
#define TEST_SUMMARY() test_print_stats()

// 测试结果宏
#define TEST_RESULT() test_get_result()

// 断言宏
#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        printf("Assertion failed: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        printf("Assertion failed: %s should be true\n", #condition); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_FALSE(condition) do { \
    if (condition) { \
        printf("Assertion failed: %s should be false\n", #condition); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_EQ(actual, expected) do { \
    if ((actual) != (expected)) { \
        printf("Assertion failed: %s == %s\n", #actual, #expected); \
        printf("  actual: %ld, expected: %ld\n", (long)(actual), (long)(expected)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_NE(actual, expected) do { \
    if ((actual) == (expected)) { \
        printf("Assertion failed: %s != %s\n", #actual, #expected); \
        printf("  actual: %ld, expected: %ld\n", (long)(actual), (long)(expected)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_GT(actual, expected) do { \
    if ((actual) <= (expected)) { \
        printf("Assertion failed: %s > %s\n", #actual, #expected); \
        printf("  actual: %ld, expected: %ld\n", (long)(actual), (long)(expected)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_GE(actual, expected) do { \
    if ((actual) < (expected)) { \
        printf("Assertion failed: %s >= %s\n", #actual, #expected); \
        printf("  actual: %ld, expected: %ld\n", (long)(actual), (long)(expected)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_LT(actual, expected) do { \
    if ((actual) >= (expected)) { \
        printf("Assertion failed: %s < %s\n", #actual, #expected); \
        printf("  actual: %ld, expected: %ld\n", (long)(actual), (long)(expected)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_LE(actual, expected) do { \
    if ((actual) > (expected)) { \
        printf("Assertion failed: %s <= %s\n", #actual, #expected); \
        printf("  actual: %ld, expected: %ld\n", (long)(actual), (long)(expected)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        printf("Assertion failed: %s is not NULL\n", #ptr); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("Assertion failed: %s is NULL\n", #ptr); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_STR_EQ(actual, expected) do { \
    if (strcmp((actual), (expected)) != 0) { \
        printf("Assertion failed: strcmp(%s, %s) == 0\n", #actual, #expected); \
        printf("  actual: \"%s\"\n  expected: \"%s\"\n", (actual), (expected)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_STR_NE(actual, expected) do { \
    if (strcmp((actual), (expected)) == 0) { \
        printf("Assertion failed: strcmp(%s, %s) != 0\n", #actual, #expected); \
        printf("  actual: \"%s\"\n  expected: \"%s\"\n", (actual), (expected)); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_MEM_EQ(actual, expected, size) do { \
    if (memcmp((actual), (expected), (size)) != 0) { \
        printf("Assertion failed: memcmp(%s, %s, %s) == 0\n", #actual, #expected, #size); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#endif // PPDB_TEST_MACROS_H 