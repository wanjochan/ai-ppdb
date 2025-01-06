#include "test.h"
#include "internal/base.h"

static void test_async_loop(void) {
    ppdb_base_async_loop_t* loop = NULL;
    ppdb_error_t err;

    // Test loop creation
    err = ppdb_base_async_loop_create(&loop);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(loop);

    // Test loop destruction
    err = ppdb_base_async_loop_destroy(loop);
    ASSERT_EQ(err, PPDB_OK);
}

static void test_async_handle(void) {
    ppdb_base_async_loop_t* loop = NULL;
    ppdb_base_async_handle_t* handle = NULL;
    ppdb_error_t err;

    // Create loop
    err = ppdb_base_async_loop_create(&loop);
    ASSERT_EQ(err, PPDB_OK);

    // Test handle creation
    int fd = open("/dev/null", O_RDWR);
    ASSERT_GT(fd, 0);

    err = ppdb_base_async_handle_create(loop, fd, &handle);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(handle);

    // Test handle destruction
    err = ppdb_base_async_handle_destroy(handle);
    ASSERT_EQ(err, PPDB_OK);

    close(fd);
    ppdb_base_async_loop_destroy(loop);
}

static void test_future(void) {
    ppdb_base_async_loop_t* loop = NULL;
    ppdb_base_async_future_t* future = NULL;
    ppdb_error_t err;

    // Create loop
    err = ppdb_base_async_loop_create(&loop);
    ASSERT_EQ(err, PPDB_OK);

    // Test future creation
    err = ppdb_base_future_create(loop, &future);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(future);

    // Test setting result
    int result = 42;
    err = ppdb_base_future_set_result(future, &result, sizeof(result));
    ASSERT_EQ(err, PPDB_OK);

    // Test getting result
    int value;
    size_t size;
    err = ppdb_base_future_get_result(future, &value, sizeof(value), &size);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_EQ(size, sizeof(value));
    ASSERT_EQ(value, result);

    // Test future destruction
    err = ppdb_base_future_destroy(future);
    ASSERT_EQ(err, PPDB_OK);

    ppdb_base_async_loop_destroy(loop);
}

static void timer_callback(ppdb_base_async_handle_t* handle, int status) {
    int* counter = (int*)handle->data;
    (*counter)++;
}

static void test_timer(void) {
    ppdb_base_async_loop_t* loop = NULL;
    ppdb_base_timer_t* timer = NULL;
    ppdb_error_t err;

    // Create loop
    err = ppdb_base_async_loop_create(&loop);
    ASSERT_EQ(err, PPDB_OK);

    // Test timer creation
    err = ppdb_base_timer_create(loop, &timer);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(timer);

    // Test starting timer
    int counter = 0;
    timer->handle->data = &counter;
    err = ppdb_base_timer_start(timer, 100, false, timer_callback, NULL);
    ASSERT_EQ(err, PPDB_OK);

    // Run loop for a short time
    err = ppdb_base_async_loop_run(loop, 200);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_EQ(counter, 1);

    // Test stopping timer
    err = ppdb_base_timer_stop(timer);
    ASSERT_EQ(err, PPDB_OK);

    // Test timer destruction
    err = ppdb_base_timer_destroy(timer);
    ASSERT_EQ(err, PPDB_OK);

    ppdb_base_async_loop_destroy(loop);
}

int main(void) {
    TEST_INIT("Base Async");

    RUN_TEST(test_async_loop);
    RUN_TEST(test_async_handle);
    RUN_TEST(test_future);
    RUN_TEST(test_timer);

    TEST_SUMMARY();
    return 0;
}