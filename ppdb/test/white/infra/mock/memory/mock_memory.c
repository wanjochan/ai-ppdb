#include "mock_memory.h"
#include "framework/mock_framework/mock_framework.h"
#include "internal/infra/infra_core.h"

// 原始函数指针
void* (*real_infra_malloc)(size_t size);
void (*real_infra_free)(void* ptr);
void* (*real_infra_memset)(void* s, int c, size_t n);
void* (*real_infra_memcpy)(void* dest, const void* src, size_t n);
void* (*real_infra_memmove)(void* dest, const void* src, size_t n);

// Mock expectation getters
mock_expectation_t* mock_expect_infra_malloc(void) {
    return mock_register_expectation("infra_malloc");
}

mock_expectation_t* mock_expect_infra_free(void) {
    return mock_register_expectation("infra_free");
}

mock_expectation_t* mock_expect_infra_memset(void) {
    return mock_register_expectation("infra_memset");
}

mock_expectation_t* mock_expect_infra_memcpy(void) {
    return mock_register_expectation("infra_memcpy");
}

mock_expectation_t* mock_expect_infra_memmove(void) {
    return mock_register_expectation("infra_memmove");
}

// Mock 函数实现
void* mock_infra_malloc(size_t size) {
    mock_expectation_t* exp = mock_register_expectation("infra_malloc");
    if (exp) {
        exp->actual_calls++;
        return exp->return_value;
    }
    return real_infra_malloc(size);
}

void mock_infra_free(void* ptr) {
    mock_expectation_t* exp = mock_register_expectation("infra_free");
    if (exp) {
        exp->actual_calls++;
        return;
    }
    real_infra_free(ptr);
}

void* mock_infra_memset(void* s, int c, size_t n) {
    mock_expectation_t* exp = mock_register_expectation("infra_memset");
    if (exp) {
        exp->actual_calls++;
        return exp->return_value;
    }
    return real_infra_memset(s, c, n);
}

void* mock_infra_memcpy(void* dest, const void* src, size_t n) {
    mock_expectation_t* exp = mock_register_expectation("infra_memcpy");
    if (exp) {
        exp->actual_calls++;
        return exp->return_value;
    }
    return real_infra_memcpy(dest, src, n);
}

void* mock_infra_memmove(void* dest, const void* src, size_t n) {
    mock_expectation_t* exp = mock_register_expectation("infra_memmove");
    if (exp) {
        exp->actual_calls++;
        return exp->return_value;
    }
    return real_infra_memmove(dest, src, n);
}

// 初始化 memory mocks
mock_error_t mock_memory_init(void) {
    // 保存原始函数指针
    real_infra_malloc = (void* (*)(size_t))infra_malloc;
    real_infra_free = (void (*)(void*))infra_free;
    real_infra_memset = (void* (*)(void*, int, size_t))infra_memset;
    real_infra_memcpy = (void* (*)(void*, const void*, size_t))infra_memcpy;
    real_infra_memmove = (void* (*)(void*, const void*, size_t))infra_memmove;

    // 替换为 mock 函数
    *(void**)&infra_malloc = (void*)mock_infra_malloc;
    *(void**)&infra_free = (void*)mock_infra_free;
    *(void**)&infra_memset = (void*)mock_infra_memset;
    *(void**)&infra_memcpy = (void*)mock_infra_memcpy;
    *(void**)&infra_memmove = (void*)mock_infra_memmove;

    return mock_framework_init();
}

// 清理 memory mocks
void mock_memory_cleanup(void) {
    // 恢复原始函数
    *(void**)&infra_malloc = (void*)real_infra_malloc;
    *(void**)&infra_free = (void*)real_infra_free;
    *(void**)&infra_memset = (void*)real_infra_memset;
    *(void**)&infra_memcpy = (void*)real_infra_memcpy;
    *(void**)&infra_memmove = (void*)real_infra_memmove;

    mock_framework_cleanup();
}