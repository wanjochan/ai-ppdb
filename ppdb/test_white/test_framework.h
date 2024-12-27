#ifndef PPDB_TEST_FRAMEWORK_H
#define PPDB_TEST_FRAMEWORK_H

#include <cosmopolitan.h>
#include <ppdb/error.h>
#include <ppdb/logger.h>

// Test case function type
typedef int (*test_case_fn_t)(void);

// Test case structure
typedef struct {
    const char* name;
    test_case_fn_t fn;
} test_case_t;

// Test suite structure
typedef struct {
    const char* name;
    const test_case_t* cases;
    size_t num_cases;
} test_suite_t;

// Run a test suite
int run_test_suite(const test_suite_t* suite);

// Clean up test directory
void cleanup_test_dir(const char* dir_path);

// Test assertion macros
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            ppdb_log_error("Assertion failed: %s", message); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_OK(err, message) \
    do { \
        if ((err) != PPDB_OK) { \
            ppdb_log_error("Operation failed: %s (error: %d)", message, err); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            ppdb_log_error("Null pointer: %s", message); \
            return 1; \
        } \
    } while (0)

#endif // PPDB_TEST_FRAMEWORK_H