#include "test/white/framework/test_framework.h"
#include "test/white/framework/mock_framework.h"
#include "internal/infra/infra_core.h"
#include "cosmopolitan.h"

void* mock_malloc(size_t size) {
    mock_function_call("mock_malloc");
    mock_param_value("size", size);
    return mock_return_ptr("mock_malloc");
}

void mock_free(void* ptr) {
    mock_function_call("mock_free");
    mock_param_ptr("ptr", ptr);
}

int mock_strcmp(const char* s1, const char* s2) {
    mock_function_call("mock_strcmp");
    mock_param_str("s1", s1);
    mock_param_str("s2", s2);
    return mock_return_value("mock_strcmp");
}

void* mock_memset(void* s, int c, size_t n) {
    mock_function_call("mock_memset");
    mock_param_ptr("s", s);
    mock_param_value("c", c);
    mock_param_value("n", n);
    return mock_return_ptr("mock_memset");
}

void* mock_memcpy(void* dest, const void* src, size_t n) {
    mock_function_call("mock_memcpy");
    mock_param_ptr("dest", dest);
    mock_param_ptr("src", src);
    mock_param_value("n", n);
    return mock_return_ptr("mock_memcpy");
}

void* mock_memmove(void* dest, const void* src, size_t n) {
    mock_function_call("mock_memmove");
    mock_param_ptr("dest", dest);
    mock_param_ptr("src", src);
    mock_param_value("n", n);
    return mock_return_ptr("mock_memmove");
}

infra_time_t mock_time_monotonic(void) {
    mock_function_call("mock_time_monotonic");
    return mock_return_value("mock_time_monotonic");
}

void mock_log(infra_log_level_t level, const char* format, ...) {
    mock_function_call("mock_log");
    mock_param_value("level", level);
    mock_param_str("format", format);
}

int mock_vsnprintf(char* str, size_t size, const char* format, va_list args) {
    mock_function_call("mock_vsnprintf");
    mock_param_ptr("str", str);
    mock_param_value("size", size);
    mock_param_str("format", format);
    return mock_return_value("mock_vsnprintf");
} 