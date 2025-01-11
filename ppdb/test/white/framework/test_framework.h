#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include "cosmopolitan.h"
#include "internal/infra/infra.h"

// Utility macros
#define INFRA_UNUSED(x) ((void)(x))

typedef void (*test_func_t)(void);

extern int g_test_stats[3];  // total, passed, failed

#define TEST_STATS_TOTAL   0
#define TEST_STATS_PASSED  1
#define TEST_STATS_FAILED  2

#define TEST_BEGIN() \
    test_init(); \
    printf("\nRunning tests...\n");

#define TEST_END() \
    test_report(); \
    test_cleanup(); \
    return g_test_stats[TEST_STATS_FAILED] ? 1 : 0;

#define RUN_TEST(test_func) \
    do { \
        int failed_before = g_test_stats[TEST_STATS_FAILED]; \
        printf("\nRunning test: %s\n", #test_func); \
        g_test_stats[TEST_STATS_TOTAL]++; \
        test_func(); \
        if (g_test_stats[TEST_STATS_FAILED] == failed_before) { \
            g_test_stats[TEST_STATS_PASSED]++; \
            printf("  PASS\n"); \
        } \
    } while (0)

#define TEST_ASSERT_MSG_VOID(condition, format, ...) \
    do { \
        if (!(condition)) { \
            g_test_stats[TEST_STATS_FAILED]++; \
            printf("Assertion failed at %s:%d: " format "\n", \
                   __FILE__, __LINE__, ##__VA_ARGS__); \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_MSG_PTR(condition, format, ...) \
    do { \
        if (!(condition)) { \
            g_test_stats[TEST_STATS_FAILED]++; \
            printf("Assertion failed at %s:%d: " format "\n", \
                   __FILE__, __LINE__, ##__VA_ARGS__); \
            return NULL; \
        } \
    } while (0)

#define TEST_ASSERT_MSG(condition, format, ...) TEST_ASSERT_MSG_VOID(condition, format, ##__VA_ARGS__)
#define TEST_ASSERT(condition) TEST_ASSERT_MSG(condition, "%s", #condition)

#define TEST_ASSERT_EQUAL(expected, actual) \
    TEST_ASSERT_MSG((expected) == (actual), \
                    "Expected %lu but got %lu", \
                    (uint64_t)(expected), (uint64_t)(actual))

#define TEST_ASSERT_EQUAL_PTR(expected, actual) \
    TEST_ASSERT_MSG((expected) == (actual), \
                    "Expected pointer %p but got %p", \
                    (void*)(expected), (void*)(actual))

#define TEST_ASSERT_EQUAL_STR(expected, actual) \
    TEST_ASSERT_MSG(infra_strcmp(expected, actual) == 0, \
                    "Expected string \"%s\" but got \"%s\"", \
                    expected, actual)

#define TEST_ASSERT_NULL(x) \
    TEST_ASSERT_MSG((x) == NULL, "%s is not NULL", #x)

#define TEST_ASSERT_NOT_NULL(x) \
    TEST_ASSERT_MSG((x) != NULL, "%s is NULL", #x)

#define TEST_ASSERT_TRUE(x) \
    TEST_ASSERT_MSG((x), "%s is not true", #x)

#define TEST_ASSERT_FALSE(x) \
    TEST_ASSERT_MSG(!(x), "%s is not false", #x)

void test_init(void);
void test_cleanup(void);
void test_report(void);

#endif /* TEST_FRAMEWORK_H */