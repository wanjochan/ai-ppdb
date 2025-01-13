#include "test/white/framework/mock_framework.h"
#include "test/white/infra/mock_memory.h"
#include "src/internal/infra/infra_core.h"

void* mock_malloc(size_t size) {
    mock_function_call("mock_malloc");
    mock_param_value("size", size);
    return mock_return_ptr("mock_malloc");
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