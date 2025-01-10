#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include "internal/infra/infra.h"

// Test case type
typedef struct {
    const char* name;
    int (*fn)(void);
} test_case_t;

// Test statistics
static struct {
    int total_tests;
    int failed_tests;
    infra_time_t start_time;
} test_stats = {0, 0, 0};

// Test macros
#define TEST_INIT() do { \
    test_stats.total_tests = 0; \
    test_stats.failed_tests = 0; \
    test_stats.start_time = infra_time_monotonic(); \
} while(0)

#define TEST_CLEANUP() do { \
    infra_time_t end_time = infra_time_monotonic(); \
    double time_spent = (double)(end_time - test_stats.start_time) / 1000000.0; \
    printf("\nTest Summary:\n"); \
    printf("Total tests: %d\n", test_stats.total_tests); \
    printf("Failed tests: %d\n", test_stats.failed_tests); \
    printf("Time spent: %.2f seconds\n", time_spent); \
} while(0)

#define TEST_RUN(test_fn) run_test_case(&(test_case_t){.name = #test_fn, .fn = test_fn})

// Test assertion macros
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "Assertion failed: %s\n", #condition); \
            fprintf(stderr, "  at %s:%d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

#define TEST_ASSERT_EQUALS(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "Assertion failed: %s != %s\n", #expected, #actual); \
            fprintf(stderr, "  at %s:%d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_OK(expr) \
    do { \
        infra_error_t __err = (expr); \
        if (__err != INFRA_OK) { \
            fprintf(stderr, "Error %d at %s:%d: %s failed\n", \
                    __err, __FILE__, __LINE__, #expr); \
            exit(1); \
        } \
    } while (0)

// Function declarations
int run_test_case(const test_case_t* test_case);

#endif /* TEST_FRAMEWORK_H */