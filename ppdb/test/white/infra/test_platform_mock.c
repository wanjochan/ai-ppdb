#include "test/white/framework/test_framework.h"
#include "test/white/framework/mock_framework.h"
#include "test/white/infra/mock_platform.h"

void test_mock_time_monotonic(void) {
    infra_time_t time = 123456789;

    mock_expect_function_call("mock_time_monotonic");
    mock_expect_return_value("mock_time_monotonic", time);

    infra_time_t result = mock_time_monotonic();
    TEST_ASSERT_EQUAL(time, result);

    mock_verify();
}

void test_mock_thread_create(void) {
    infra_thread_t thread;
    infra_thread_attr_t attr;
    void* (*func)(void*) = (void* (*)(void*))0x12345678;
    void* arg = (void*)0x87654321;

    mock_expect_function_call("mock_thread_create");
    mock_expect_param_ptr("thread", &thread);
    mock_expect_param_ptr("attr", &attr);
    mock_expect_param_ptr("func", (void*)func);
    mock_expect_param_ptr("arg", arg);
    mock_expect_return_value("mock_thread_create", 0);

    int result = mock_thread_create(&thread, &attr, func, arg);
    TEST_ASSERT_EQUAL(0, result);

    mock_verify();
}

void test_mock_mutex_operations(void) {
    infra_mutex_t mutex;
    infra_mutex_attr_t attr;

    // Test mutex init
    mock_expect_function_call("mock_mutex_init");
    mock_expect_param_ptr("mutex", &mutex);
    mock_expect_param_ptr("attr", &attr);
    mock_expect_return_value("mock_mutex_init", 0);

    int result = mock_mutex_init(&mutex, &attr);
    TEST_ASSERT_EQUAL(0, result);

    // Test mutex lock
    mock_expect_function_call("mock_mutex_lock");
    mock_expect_param_ptr("mutex", &mutex);
    mock_expect_return_value("mock_mutex_lock", 0);

    result = mock_mutex_lock(&mutex);
    TEST_ASSERT_EQUAL(0, result);

    // Test mutex unlock
    mock_expect_function_call("mock_mutex_unlock");
    mock_expect_param_ptr("mutex", &mutex);
    mock_expect_return_value("mock_mutex_unlock", 0);

    result = mock_mutex_unlock(&mutex);
    TEST_ASSERT_EQUAL(0, result);

    mock_verify();
}

void test_mock_cond_operations(void) {
    infra_cond_t cond;
    infra_mutex_t mutex;
    infra_cond_attr_t attr;

    // Test condition init
    mock_expect_function_call("mock_cond_init");
    mock_expect_param_ptr("cond", &cond);
    mock_expect_param_ptr("attr", &attr);
    mock_expect_return_value("mock_cond_init", 0);

    int result = mock_cond_init(&cond, &attr);
    TEST_ASSERT_EQUAL(0, result);

    // Test condition wait
    mock_expect_function_call("mock_cond_wait");
    mock_expect_param_ptr("cond", &cond);
    mock_expect_param_ptr("mutex", &mutex);
    mock_expect_return_value("mock_cond_wait", 0);

    result = mock_cond_wait(&cond, &mutex);
    TEST_ASSERT_EQUAL(0, result);

    // Test condition signal
    mock_expect_function_call("mock_cond_signal");
    mock_expect_param_ptr("cond", &cond);
    mock_expect_return_value("mock_cond_signal", 0);

    result = mock_cond_signal(&cond);
    TEST_ASSERT_EQUAL(0, result);

    mock_verify();
}

int main(void) {
    TEST_BEGIN("Platform Mock Tests");

    RUN_TEST(test_mock_time_monotonic);
    RUN_TEST(test_mock_thread_create);
    RUN_TEST(test_mock_mutex_operations);
    RUN_TEST(test_mock_cond_operations);

    TEST_END();
    return 0;
} 