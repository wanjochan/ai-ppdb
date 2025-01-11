#include "test/white/framework/test_framework.h"
#include "test/white/framework/mock_framework.h"
#include "test/white/infra/mock_memory.h"

void test_mock_malloc(void) {
    void* ptr = (void*)0x12345678;

    mock_expect_function_call("mock_malloc");
    mock_expect_param_value("size", 42);
    mock_expect_return_ptr("mock_malloc", ptr);

    void* result = mock_malloc(42);
    TEST_ASSERT_EQUAL_PTR(ptr, result);

    mock_verify();
}

void test_mock_free(void) {
    void* ptr = (void*)0x12345678;

    mock_expect_function_call("mock_free");
    mock_expect_param_ptr("ptr", ptr);

    mock_free(ptr);

    mock_verify();
}

void test_mock_memset(void) {
    char buffer[10];
    void* result = buffer;

    mock_expect_function_call("mock_memset");
    mock_expect_param_ptr("s", buffer);
    mock_expect_param_value("c", 0);
    mock_expect_param_value("n", sizeof(buffer));
    mock_expect_return_ptr("mock_memset", result);

    void* ptr = mock_memset(buffer, 0, sizeof(buffer));
    TEST_ASSERT_EQUAL_PTR(result, ptr);

    mock_verify();
}

void test_mock_memcpy(void) {
    char src[10] = "test";
    char dest[10];
    void* result = dest;

    mock_expect_function_call("mock_memcpy");
    mock_expect_param_ptr("dest", dest);
    mock_expect_param_ptr("src", src);
    mock_expect_param_value("n", 5);
    mock_expect_return_ptr("mock_memcpy", result);

    void* ptr = mock_memcpy(dest, src, 5);
    TEST_ASSERT_EQUAL_PTR(result, ptr);

    mock_verify();
}

void test_mock_memmove(void) {
    char buffer[10] = "test";
    void* result = buffer + 2;

    mock_expect_function_call("mock_memmove");
    mock_expect_param_ptr("dest", buffer + 2);
    mock_expect_param_ptr("src", buffer);
    mock_expect_param_value("n", 5);
    mock_expect_return_ptr("mock_memmove", result);

    void* ptr = mock_memmove(buffer + 2, buffer, 5);
    TEST_ASSERT_EQUAL_PTR(result, ptr);

    mock_verify();
}

int main(void) {
    TEST_BEGIN("Memory Mock Tests");

    RUN_TEST(test_mock_malloc);
    RUN_TEST(test_mock_free);
    RUN_TEST(test_mock_memset);
    RUN_TEST(test_mock_memcpy);
    RUN_TEST(test_mock_memmove);

    TEST_END();
    return 0;
} 