/*
 * test_common.h - Common test utilities
 */

#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include "internal/infra/infra.h"

// Utility macros
#define INFRA_UNUSED(x) ((void)(x))

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

#define ASSERT_EQ(x, y) \
    do { \
        if ((x) != (y)) { \
            printf("  Test failed: %s:%d: %s != %s\n", __FILE__, __LINE__, #x, #y); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_STR_EQ(x, y) \
    do { \
        if (strcmp((x), (y)) != 0) { \
            printf("  Test failed: %s:%d: %s != %s\n", __FILE__, __LINE__, #x, #y); \
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

#endif // TEST_COMMON_H_
