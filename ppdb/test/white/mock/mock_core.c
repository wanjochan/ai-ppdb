#include "test/white/mock/mock_core.h"
#include "test/white/framework/mock_framework.h"
#include <stdarg.h>

// Mock function implementations for memory operations
void* mock_malloc(size_t size) {
    mock_function_call("mock_malloc");
    mock_param_value("size", size);
    return mock_return_ptr("mock_malloc");
}

void* mock_calloc(size_t nmemb, size_t size) {
    mock_function_call("mock_calloc");
    mock_param_value("nmemb", nmemb);
    mock_param_value("size", size);
    return mock_return_ptr("mock_calloc");
}

void* mock_realloc(void* ptr, size_t size) {
    mock_function_call("mock_realloc");
    mock_param_ptr("ptr", ptr);
    mock_param_value("size", size);
    return mock_return_ptr("mock_realloc");
}

void mock_free(void* ptr) {
    mock_function_call("mock_free");
    mock_param_ptr("ptr", ptr);
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

// Mock function implementations for string operations
size_t mock_strlen(const char* s) {
    mock_function_call("mock_strlen");
    mock_param_str("s", s);
    return mock_return_value("mock_strlen");
}

char* mock_strcpy(char* dest, const char* src) {
    mock_function_call("mock_strcpy");
    mock_param_ptr("dest", dest);
    mock_param_str("src", src);
    return mock_return_ptr("mock_strcpy");
}

char* mock_strncpy(char* dest, const char* src, size_t n) {
    mock_function_call("mock_strncpy");
    mock_param_ptr("dest", dest);
    mock_param_str("src", src);
    mock_param_value("n", n);
    return mock_return_ptr("mock_strncpy");
}

char* mock_strcat(char* dest, const char* src) {
    mock_function_call("mock_strcat");
    mock_param_ptr("dest", dest);
    mock_param_str("src", src);
    return mock_return_ptr("mock_strcat");
}

char* mock_strncat(char* dest, const char* src, size_t n) {
    mock_function_call("mock_strncat");
    mock_param_ptr("dest", dest);
    mock_param_str("src", src);
    mock_param_value("n", n);
    return mock_return_ptr("mock_strncat");
}

int mock_strcmp(const char* s1, const char* s2) {
    mock_function_call("mock_strcmp");
    mock_param_str("s1", s1);
    mock_param_str("s2", s2);
    return mock_return_value("mock_strcmp");
}

int mock_strncmp(const char* s1, const char* s2, size_t n) {
    mock_function_call("mock_strncmp");
    mock_param_str("s1", s1);
    mock_param_str("s2", s2);
    mock_param_value("n", n);
    return mock_return_value("mock_strncmp");
}

char* mock_strdup(const char* s) {
    mock_function_call("mock_strdup");
    mock_param_str("s", s);
    return mock_return_ptr("mock_strdup");
}

// Mock function implementations for time operations
infra_time_t mock_time_now(void) {
    mock_function_call("mock_time_now");
    return mock_return_value("mock_time_now");
}

infra_time_t mock_time_monotonic(void) {
    mock_function_call("mock_time_monotonic");
    return mock_return_value("mock_time_monotonic");
}

void mock_time_sleep(uint32_t ms) {
    mock_function_call("mock_time_sleep");
    mock_param_value("ms", ms);
}

void mock_time_yield(void) {
    mock_function_call("mock_time_yield");
}

// Mock function implementations for logging
void mock_log(int level, const char* file, int line, const char* func, const char* format, ...) {
    mock_function_call("mock_log");
    mock_param_value("level", level);
    mock_param_str("file", file);
    mock_param_value("line", line);
    mock_param_str("func", func);
    mock_param_str("format", format);
    
    va_list args;
    va_start(args, format);
    char message[1024];
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    mock_param_str("message", message);
}

void mock_log_set_level(int level) {
    mock_function_call("mock_log_set_level");
    mock_param_value("level", level);
}

// Mock control functions
void mock_core_init(void) {
    mock_init();
}

void mock_core_verify(void) {
    mock_verify();
}

void mock_core_cleanup(void) {
    mock_cleanup();
} 