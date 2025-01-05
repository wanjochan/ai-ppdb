#include "test_common.h"
#include "../../internal/core.h"

static void test_async_loop_basic(void) {
    ppdb_core_async_loop_t* loop;
    TEST_ASSERT_OK(ppdb_core_async_loop_create(&loop));
    TEST_ASSERT_OK(ppdb_core_async_loop_destroy(loop));
}

static void async_timer_callback(ppdb_core_async_handle_t* handle, int status) {
    atomic_size_t* counter = handle->data;
    atomic_fetch_add(counter, 1);
}

static void test_async_timer(void) {
    ppdb_core_async_loop_t* loop;
    ppdb_core_timer_t* timer;
    atomic_size_t counter = 0;
    
    TEST_ASSERT_OK(ppdb_core_async_loop_create(&loop));
    TEST_ASSERT_OK(ppdb_core_timer_create(loop, &timer));
    
    // Start a timer that fires every 10ms
    TEST_ASSERT_OK(ppdb_core_timer_start(timer, 10, true, async_timer_callback, &counter));
    
    // Run the loop for 100ms
    TEST_ASSERT_OK(ppdb_core_async_loop_run(loop, 100));
    
    // Should have fired approximately 10 times
    TEST_ASSERT_TRUE(atomic_load(&counter) >= 8 && atomic_load(&counter) <= 12);
    
    TEST_ASSERT_OK(ppdb_core_timer_destroy(timer));
    TEST_ASSERT_OK(ppdb_core_async_loop_destroy(loop));
}

static void async_read_callback(ppdb_core_async_handle_t* handle, int status) {
    ppdb_core_future_t* future = handle->data;
    if (status > 0) {
        ppdb_core_future_set_result(future, handle->io.buf, status);
    } else {
        ppdb_core_future_set_error(future, PPDB_ERR_IO);
    }
}

static void test_async_io(void) {
    ppdb_core_async_loop_t* loop;
    ppdb_core_async_handle_t* handle;
    ppdb_core_future_t* future;
    char buf[128];
    size_t bytes_read;
    
    TEST_ASSERT_OK(ppdb_core_async_loop_create(&loop));
    TEST_ASSERT_OK(ppdb_core_async_handle_create(loop, STDIN_FILENO, &handle));
    TEST_ASSERT_OK(ppdb_core_future_create(loop, &future));
    
    // Start async read
    handle->data = future;
    TEST_ASSERT_OK(ppdb_core_async_read(handle, buf, sizeof(buf), async_read_callback));
    
    // Run loop with timeout
    TEST_ASSERT_OK(ppdb_core_async_loop_run(loop, 100));
    
    // Check result
    bool ready;
    TEST_ASSERT_OK(ppdb_core_future_is_ready(future, &ready));
    if (ready) {
        TEST_ASSERT_OK(ppdb_core_future_get_result(future, buf, sizeof(buf), &bytes_read));
        TEST_ASSERT_TRUE(bytes_read > 0);
    }
    
    TEST_ASSERT_OK(ppdb_core_future_destroy(future));
    TEST_ASSERT_OK(ppdb_core_async_handle_destroy(handle));
    TEST_ASSERT_OK(ppdb_core_async_loop_destroy(loop));
}

static void test_future_basic(void) {
    ppdb_core_async_loop_t* loop;
    ppdb_core_future_t* future;
    int value = 42;
    int result;
    size_t size;
    bool ready;
    
    TEST_ASSERT_OK(ppdb_core_async_loop_create(&loop));
    TEST_ASSERT_OK(ppdb_core_future_create(loop, &future));
    
    // Test not ready
    TEST_ASSERT_OK(ppdb_core_future_is_ready(future, &ready));
    TEST_ASSERT_FALSE(ready);
    
    // Set result
    TEST_ASSERT_OK(ppdb_core_future_set_result(future, &value, sizeof(value)));
    
    // Test ready
    TEST_ASSERT_OK(ppdb_core_future_is_ready(future, &ready));
    TEST_ASSERT_TRUE(ready);
    
    // Get result
    TEST_ASSERT_OK(ppdb_core_future_get_result(future, &result, sizeof(result), &size));
    TEST_ASSERT_EQUAL(sizeof(int), size);
    TEST_ASSERT_EQUAL(42, result);
    
    TEST_ASSERT_OK(ppdb_core_future_destroy(future));
    TEST_ASSERT_OK(ppdb_core_async_loop_destroy(loop));
}

static void future_callback(ppdb_core_async_handle_t* handle, int status) {
    atomic_size_t* counter = handle->data;
    atomic_fetch_add(counter, 1);
}

static void test_future_callback(void) {
    ppdb_core_async_loop_t* loop;
    ppdb_core_future_t* future;
    atomic_size_t counter = 0;
    int value = 42;
    
    TEST_ASSERT_OK(ppdb_core_async_loop_create(&loop));
    TEST_ASSERT_OK(ppdb_core_future_create(loop, &future));
    
    // Set callback
    TEST_ASSERT_OK(ppdb_core_future_set_callback(future, future_callback, &counter));
    
    // Set result should trigger callback
    TEST_ASSERT_OK(ppdb_core_future_set_result(future, &value, sizeof(value)));
    
    // Run loop to process callbacks
    TEST_ASSERT_OK(ppdb_core_async_loop_run(loop, 100));
    
    TEST_ASSERT_EQUAL(1, atomic_load(&counter));
    
    TEST_ASSERT_OK(ppdb_core_future_destroy(future));
    TEST_ASSERT_OK(ppdb_core_async_loop_destroy(loop));
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_async_loop_basic);
    RUN_TEST(test_async_timer);
    RUN_TEST(test_async_io);
    RUN_TEST(test_future_basic);
    RUN_TEST(test_future_callback);
    
    return UNITY_END();
}
