#include "test/white/framework/test_framework.h"
#include "test/white/mock/mock_core.h"
#include "internal/infra/infra_core.h"

// Test cases for memory operations
void test_mock_malloc(void) {
    mock_core_init();
    
    // Test case 1: Successful allocation
    void* ptr = infra_malloc(100);
    mock_expect_function_call("mock_malloc");
    mock_expect_param_value("size", 100);
    mock_expect_return_ptr("mock_malloc", ptr);
    
    // Test case 2: Failed allocation
    mock_expect_function_call("mock_malloc");
    mock_expect_param_value("size", 200);
    mock_expect_return_ptr("mock_malloc", NULL);
    
    mock_core_verify();
    mock_core_cleanup();
}

void test_mock_free(void) {
    mock_core_init();
    
    void* ptr = (void*)0x12345678;
    infra_free(ptr);
    mock_expect_function_call("mock_free");
    mock_expect_param_ptr("ptr", ptr);
    
    mock_core_verify();
    mock_core_cleanup();
}

// Test cases for string operations
void test_mock_strcmp(void) {
    mock_core_init();
    
    const char* s1 = "hello";
    const char* s2 = "world";
    
    // Test case 1: Strings are equal
    mock_expect_function_call("mock_strcmp");
    mock_expect_param_str("s1", s1);
    mock_expect_param_str("s2", s1);
    mock_expect_return_value("mock_strcmp", 0);
    
    // Test case 2: First string is less than second
    mock_expect_function_call("mock_strcmp");
    mock_expect_param_str("s1", s1);
    mock_expect_param_str("s2", s2);
    mock_expect_return_value("mock_strcmp", -1);
    
    mock_core_verify();
    mock_core_cleanup();
}

// Test cases for time operations
void test_mock_time_monotonic(void) {
    mock_core_init();
    
    // Test case 1: Normal time value
    mock_expect_function_call("mock_time_monotonic");
    mock_expect_return_value("mock_time_monotonic", 123456789);
    
    infra_time_t time = infra_time_monotonic();
    TEST_ASSERT(time == 123456789);
    
    mock_core_verify();
    mock_core_cleanup();
}

// Test cases for logging
void test_mock_log(void) {
    mock_core_init();
    
    // Test case 1: Log an error message
    mock_expect_function_call("mock_log");
    mock_expect_param_value("level", INFRA_LOG_LEVEL_ERROR);
    mock_expect_param_str("file", __FILE__);
    mock_expect_param_value("line", __LINE__ + 3);
    mock_expect_param_str("func", __func__);
    mock_expect_param_str("format", "Error: %s");
    mock_expect_param_str("message", "Error: test error");
    
    INFRA_LOG_ERROR("Error: %s", "test error");
    
    mock_core_verify();
    mock_core_cleanup();
}

// Test runner
int main(void) {
    TEST_BEGIN("Core Mock Tests");
    
    RUN_TEST(test_mock_malloc);
    RUN_TEST(test_mock_free);
    RUN_TEST(test_mock_strcmp);
    RUN_TEST(test_mock_time_monotonic);
    RUN_TEST(test_mock_log);
    
    TEST_END();
    return 0;
} 