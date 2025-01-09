#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <cosmopolitan.h>

// Test macros
#define TEST_INIT() test_framework_init()
#define TEST_CLEANUP() test_framework_cleanup()
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

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((void*)(ptr) == (void*)NULL) { \
            fprintf(stderr, "Assertion failed: %s is NULL\n", #ptr); \
            fprintf(stderr, "  at %s:%d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((void*)(ptr) != (void*)NULL) { \
            fprintf(stderr, "Assertion failed: %s is not NULL\n", #ptr); \
            fprintf(stderr, "  at %s:%d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_OK(expr) \
    do { \
        ppdb_error_t __err = (expr); \
        if (__err != PPDB_OK) { \
            fprintf(stderr, "Error %d at %s:%d: %s failed\n", \
                    __err, __FILE__, __LINE__, #expr); \
            exit(1); \
        } \
    } while (0)

// Test types
typedef enum {
    TEST_TYPE_UNIT = 1,    // Unit test
    TEST_TYPE_PERF = 2,    // Performance test
    TEST_TYPE_STRESS = 4,  // Stress test
    TEST_TYPE_ALL = 7      // All types
} test_type_t;

// Test configuration
typedef struct {
    test_type_t type;      // Test type
} test_config_t;

// Test statistics
typedef struct {
    clock_t start_time;    // Start time
    clock_t end_time;      // End time
    size_t peak_memory;    // Peak memory usage
} test_stats_t;

// Test state
typedef struct {
    bool initialized;      // Whether initialized
    test_config_t config;  // Test configuration
    test_stats_t stats;    // Test statistics
    jmp_buf timeout_jmp;   // Timeout jump point
} test_state_t;

// Test case
typedef struct {
    const char* name;           // Test name
    const char* description;    // Test description
    int (*fn)(void);           // Test function
    int timeout_seconds;        // Timeout in seconds
    bool skip;                  // Whether to skip
} test_case_t;

// Test suite
typedef struct {
    const char* name;           // Suite name
    void (*setup)(void);       // Suite setup
    void (*teardown)(void);    // Suite teardown
    const test_case_t* cases;  // Test cases array
    size_t num_cases;          // Number of test cases
} test_suite_t;

// Framework functions
void test_framework_init(void);
void test_framework_cleanup(void);
int run_test_case(const test_case_t* test_case);
int run_test_suite(const test_suite_t* suite);
bool test_framework_should_run(test_type_t type);
void test_print_stats(void);

#endif // TEST_FRAMEWORK_H