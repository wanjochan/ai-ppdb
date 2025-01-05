#ifndef PPDB_TEST_COMMON_H
#define PPDB_TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include "ppdb/internal.h"

// 测试断言宏
#define ASSERT_OK(expr) do { \
    ppdb_error_t err = (expr); \
    if (err != PPDB_OK) { \
        fprintf(stderr, "Assert failed at %s:%d: %s returned %d\n", \
                __FILE__, __LINE__, #expr, err); \
        return -1; \
    } \
} while (0)

#define ASSERT_ERR(expr, expected) do { \
    ppdb_error_t err = (expr); \
    if (err != (expected)) { \
        fprintf(stderr, "Assert failed at %s:%d: %s returned %d, expected %d\n", \
                __FILE__, __LINE__, #expr, err, expected); \
        return -1; \
    } \
} while (0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "Assert failed at %s:%d: %s is false\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

#define ASSERT_FALSE(expr) do { \
    if (expr) { \
        fprintf(stderr, "Assert failed at %s:%d: %s is true\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

#define ASSERT_NULL(expr) do { \
    if ((expr) != NULL) { \
        fprintf(stderr, "Assert failed at %s:%d: %s is not NULL\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

#define ASSERT_NOT_NULL(expr) do { \
    if ((expr) == NULL) { \
        fprintf(stderr, "Assert failed at %s:%d: %s is NULL\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

// 测试用例宏
#define TEST_CASE(test_func) do { \
    printf("Running %s...\n", #test_func); \
    if (test_func() != 0) { \
        fprintf(stderr, "Test case %s failed\n", #test_func); \
        return -1; \
    } \
    printf("%s passed\n", #test_func); \
} while (0)

#endif // PPDB_TEST_COMMON_H
