#include "test/white/framework/mock_framework.h"
#include "test/white/framework/test_framework.h"
#include "internal/infra/infra_core.h"

#define MAX_MOCK_CALLS 1024

typedef struct {
    const char* function_name;
    const char* param_name;
    union {
        uint64_t value;
        const void* ptr;
        const char* str;
    } param;
    union {
        uint64_t value;
        void* ptr;
    } return_val;
} mock_call_t;

static mock_call_t g_mock_calls[MAX_MOCK_CALLS];
static size_t g_mock_call_count = 0;
static size_t g_mock_call_index = 0;

// Mock framework initialization and cleanup
void mock_init(void) {
    g_mock_call_count = 0;
    g_mock_call_index = 0;
}

void mock_cleanup(void) {
    g_mock_call_count = 0;
    g_mock_call_index = 0;
}

void mock_verify(void) {
    TEST_ASSERT_MSG(g_mock_call_index == g_mock_call_count, 
                   "Not all expected mock calls were made (expected %zu, got %zu)",
                   g_mock_call_count, g_mock_call_index);
}

// Mock function call recording
void mock_function_call(const char* function_name) {
    TEST_ASSERT_MSG(g_mock_call_index < g_mock_call_count,
                   "Unexpected mock call to function %s",
                   function_name);
    TEST_ASSERT_MSG(infra_strcmp(g_mock_calls[g_mock_call_index].function_name, function_name) == 0,
                   "Expected call to %s but got call to %s",
                   g_mock_calls[g_mock_call_index].function_name, function_name);
}

void mock_param_value(const char* param_name, uint64_t value) {
    TEST_ASSERT_MSG(g_mock_call_index < g_mock_call_count,
                   "Unexpected parameter value for %s",
                   param_name);
    TEST_ASSERT_MSG(infra_strcmp(g_mock_calls[g_mock_call_index].param_name, param_name) == 0,
                   "Expected parameter %s but got %s",
                   g_mock_calls[g_mock_call_index].param_name, param_name);
    TEST_ASSERT_MSG(g_mock_calls[g_mock_call_index].param.value == value,
                   "Expected value %lu for parameter %s but got %lu",
                   g_mock_calls[g_mock_call_index].param.value, param_name, value);
}

void mock_param_ptr(const char* param_name, const void* ptr) {
    TEST_ASSERT_MSG(g_mock_call_index < g_mock_call_count,
                   "Unexpected parameter pointer for %s",
                   param_name);
    TEST_ASSERT_MSG(infra_strcmp(g_mock_calls[g_mock_call_index].param_name, param_name) == 0,
                   "Expected parameter %s but got %s",
                   g_mock_calls[g_mock_call_index].param_name, param_name);
    TEST_ASSERT_MSG(g_mock_calls[g_mock_call_index].param.ptr == ptr,
                   "Expected pointer %p for parameter %s but got %p",
                   g_mock_calls[g_mock_call_index].param.ptr, param_name, ptr);
}

void mock_param_str(const char* param_name, const char* str) {
    TEST_ASSERT_MSG(g_mock_call_index < g_mock_call_count,
                   "Unexpected parameter string for %s",
                   param_name);
    TEST_ASSERT_MSG(infra_strcmp(g_mock_calls[g_mock_call_index].param_name, param_name) == 0,
                   "Expected parameter %s but got %s",
                   g_mock_calls[g_mock_call_index].param_name, param_name);
    TEST_ASSERT_MSG(infra_strcmp(g_mock_calls[g_mock_call_index].param.str, str) == 0,
                   "Expected string '%s' for parameter %s but got '%s'",
                   g_mock_calls[g_mock_call_index].param.str, param_name, str);
}

// Mock function call expectations
void mock_expect_function_call(const char* function_name) {
    TEST_ASSERT_MSG(g_mock_call_count < MAX_MOCK_CALLS,
                   "Too many mock calls (max %d)",
                   MAX_MOCK_CALLS);
    g_mock_calls[g_mock_call_count].function_name = function_name;
    g_mock_call_count++;
}

void mock_expect_param_value(const char* param_name, uint64_t value) {
    TEST_ASSERT_MSG(g_mock_call_count > 0,
                   "No function call to attach parameter to");
    g_mock_calls[g_mock_call_count - 1].param_name = param_name;
    g_mock_calls[g_mock_call_count - 1].param.value = value;
}

void mock_expect_param_ptr(const char* param_name, const void* ptr) {
    TEST_ASSERT_MSG(g_mock_call_count > 0,
                   "No function call to attach parameter to");
    g_mock_calls[g_mock_call_count - 1].param_name = param_name;
    g_mock_calls[g_mock_call_count - 1].param.ptr = ptr;
}

void mock_expect_param_str(const char* param_name, const char* str) {
    TEST_ASSERT_MSG(g_mock_call_count > 0,
                   "No function call to attach parameter to");
    g_mock_calls[g_mock_call_count - 1].param_name = param_name;
    g_mock_calls[g_mock_call_count - 1].param.str = str;
}

void mock_expect_return_value(const char* function_name, uint64_t value) {
    (void)function_name;  // Suppress unused parameter warning
    g_mock_calls[g_mock_call_count].return_val.value = value;
}

void* mock_expect_return_ptr(const char* function_name, void* ptr) {
    (void)function_name;  // Suppress unused parameter warning
    g_mock_calls[g_mock_call_count].return_val.ptr = ptr;
    return ptr;
}

// Mock function return values
uint64_t mock_return_value(const char* function_name) {
    (void)function_name;  // Suppress unused parameter warning
    return g_mock_calls[g_mock_call_index].return_val.value;
}

void* mock_return_ptr(const char* function_name) {
    (void)function_name;  // Suppress unused parameter warning
    return g_mock_calls[g_mock_call_index].return_val.ptr;
} 