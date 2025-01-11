#ifndef PPDB_TEST_FRAMEWORK_MOCK_FRAMEWORK_H
#define PPDB_TEST_FRAMEWORK_MOCK_FRAMEWORK_H

#include "../test_framework.h"

// Mock framework error codes
typedef enum {
    MOCK_OK = 0,
    MOCK_ERROR_INVALID_PARAM = -1,
    MOCK_ERROR_TOO_MANY_EXPECTATIONS = -2,
    MOCK_ERROR_EXPECTATION_FAILED = -3
} mock_error_t;

// Mock expectation structure
struct mock_expectation {
    const char* func_name;
    int expected_calls;
    int actual_calls;
    void* return_value;
    struct mock_expectation* next;
};
typedef struct mock_expectation mock_expectation_t;

// Mock function registration and expectation macros
#define MOCK_FUNC(ret_type, func_name, ...) \
    extern ret_type (*real_##func_name)(__VA_ARGS__); \
    ret_type mock_##func_name(__VA_ARGS__)

// Mock expectation setup - returns mock_expectation_t*
#define EXPECT_CALL(func_name) \
    mock_expect_##func_name()

// Mock call count expectation
#define TIMES(exp, count) \
    mock_expect_times(exp, count)

// Mock return value setup
#define WILL_RETURN(exp, value) \
    mock_will_return(exp, value)

// Mock verification
#define VERIFY_CALLS() \
    mock_verify_all_expectations()

// Mock framework initialization
mock_error_t mock_framework_init(void);

// Mock framework cleanup
void mock_framework_cleanup(void);

// Register a new mock expectation
mock_expectation_t* mock_register_expectation(const char* func_name);

// Set expected call count for current mock
void mock_expect_times(mock_expectation_t* exp, int count);

// Set return value for current mock
void mock_will_return(mock_expectation_t* exp, void* value);

// Verify all mock expectations
mock_error_t mock_verify_all_expectations(void);

// Get last mock error
const char* mock_get_last_error(void);

#endif // PPDB_TEST_FRAMEWORK_MOCK_FRAMEWORK_H 