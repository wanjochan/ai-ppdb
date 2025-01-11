#include "framework/test_framework.h"
#include "mock_memory.h"

// Test memory allocation mock
static int test_memory_mock_malloc(void) {
    // 初始化 mock
    TEST_ASSERT(mock_memory_init() == MOCK_OK);

    // 设置期望：malloc 被调用一次，返回一个测试缓冲区
    char test_buffer[100];
    mock_expectation_t* exp = EXPECT_CALL(infra_malloc);
    TIMES(exp, 1);
    WILL_RETURN(exp, test_buffer);

    // 执行被测试的代码
    void* ptr = infra_malloc(100);
    TEST_ASSERT(ptr == test_buffer);

    // 验证所有 mock 期望都被满足
    TEST_ASSERT(mock_verify_all_expectations() == MOCK_OK);

    // 清理 mock
    mock_memory_cleanup();
    return 0;
}

// Test memory operation mock
static int test_memory_mock_memset(void) {
    // 初始化 mock
    TEST_ASSERT(mock_memory_init() == MOCK_OK);

    // 设置期望：memset 被调用一次
    char test_buffer[100];
    mock_expectation_t* exp = EXPECT_CALL(infra_memset);
    TIMES(exp, 1);
    WILL_RETURN(exp, test_buffer);

    // 执行被测试的代码
    void* ptr = infra_memset(test_buffer, 0, sizeof(test_buffer));
    TEST_ASSERT(ptr == test_buffer);

    // 验证所有 mock 期望都被满足
    TEST_ASSERT(mock_verify_all_expectations() == MOCK_OK);

    // 清理 mock
    mock_memory_cleanup();
    return 0;
}

// Test memory mock error cases
static int test_memory_mock_errors(void) {
    // 初始化 mock
    TEST_ASSERT(mock_memory_init() == MOCK_OK);

    // 设置期望：malloc 被调用两次，但实际只调用一次
    mock_expectation_t* exp = EXPECT_CALL(infra_malloc);
    TIMES(exp, 2);
    WILL_RETURN(exp, NULL);

    // 执行被测试的代码
    infra_malloc(100);

    // 验证期望失败（因为调用次数不匹配）
    TEST_ASSERT(mock_verify_all_expectations() == MOCK_ERROR_EXPECTATION_FAILED);
    TEST_ASSERT(infra_strcmp(mock_get_last_error(), 
        "Mock expectation failed for infra_malloc: expected 2 calls, got 1") == 0);

    // 清理 mock
    mock_memory_cleanup();
    return 0;
}

// Test memory free mock
static int test_memory_mock_free(void) {
    // 初始化 mock
    TEST_ASSERT(mock_memory_init() == MOCK_OK);

    // 设置期望：free 被调用一次
    mock_expectation_t* exp = EXPECT_CALL(infra_free);
    TIMES(exp, 1);

    // 执行被测试的代码
    void* ptr = infra_malloc(100);  // 使用真实的 malloc
    infra_free(ptr);

    // 验证所有 mock 期望都被满足
    TEST_ASSERT(mock_verify_all_expectations() == MOCK_OK);

    // 清理 mock
    mock_memory_cleanup();
    return 0;
}

int main(void) {
    TEST_INIT();

    TEST_RUN(test_memory_mock_malloc);
    TEST_RUN(test_memory_mock_memset);
    TEST_RUN(test_memory_mock_errors);
    TEST_RUN(test_memory_mock_free);

    TEST_CLEANUP();
    return 0;
} 