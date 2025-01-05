/*
 * test_common.h - Common test utilities
 */

#ifndef PPDB_TEST_COMMON_H_
#define PPDB_TEST_COMMON_H_

#include <cosmopolitan.h>

// Test macros
#define ASSERT_TRUE(x) \
    do { \
        if (!(x)) { \
            printf("  Test failed: %s:%d: %s\n", __FILE__, __LINE__, #x); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_FALSE(x) \
    do { \
        if (x) { \
            printf("  Test failed: %s:%d: !%s\n", __FILE__, __LINE__, #x); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NULL(x) \
    do { \
        if ((x) != NULL) { \
            printf("  Test failed: %s:%d: %s != NULL\n", __FILE__, __LINE__, #x); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_NOT_NULL(x) \
    do { \
        if ((x) == NULL) { \
            printf("  Test failed: %s:%d: %s == NULL\n", __FILE__, __LINE__, #x); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_EQUAL(x, y) \
    do { \
        if ((x) != (y)) { \
            printf("  Test failed: %s:%d: %s != %s\n", __FILE__, __LINE__, #x, #y); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_STR_EQUAL(x, y) \
    do { \
        if (strcmp((x), (y)) != 0) { \
            printf("  Test failed: %s:%d: %s != %s\n", __FILE__, __LINE__, #x, #y); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_OK(x) \
    do { \
        if ((x) != PPDB_OK) { \
            printf("  Test failed: %s:%d: %s != PPDB_OK\n", __FILE__, __LINE__, #x); \
            exit(1); \
        } \
    } while (0)

// Test runner macro
#define RUN_TEST(test) \
    do { \
        printf("  Running test: %s\n", #test); \
        test(); \
        printf("  Test passed: %s\n", #test); \
    } while (0)

#endif // PPDB_TEST_COMMON_H_
