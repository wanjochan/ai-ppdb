#include <cosmopolitan.h>
#include "../test_framework.h"
#include "internal/base.h"

// 测试异步循环基本功能
static int test_async_loop_basic(void) {
    ppdb_base_async_loop_t* loop = NULL;
    TEST_ASSERT_EQUALS(ppdb_base_async_loop_create(&loop), PPDB_OK);
    TEST_ASSERT_NOT_NULL(loop);
    
    TEST_ASSERT_EQUALS(ppdb_base_async_loop_destroy(loop), PPDB_OK);
    return 0;
}

// 定时器回调函数
static void timer_callback(ppdb_base_async_handle_t* handle, ppdb_error_t status) {
    int* counter = handle->data;
    (*counter)++;
}

// 测试定时器功能
static int test_async_timer(void) {
    ppdb_base_async_loop_t* loop = NULL;
    ppdb_base_timer_t* timer = NULL;
    int counter = 0;
    
    // 创建事件循环
    TEST_ASSERT_EQUALS(ppdb_base_async_loop_create(&loop), PPDB_OK);
    
    // 创建定时器
    TEST_ASSERT_EQUALS(ppdb_base_timer_create(loop, &timer), PPDB_OK);
    timer->data = &counter;
    
    // 启动定时器
    TEST_ASSERT_EQUALS(ppdb_base_timer_start(timer, 10, true, timer_callback), PPDB_OK);
    
    // 运行事件循环
    TEST_ASSERT_EQUALS(ppdb_base_async_loop_run(loop, 100), PPDB_OK);
    
    // 验证回调次数
    TEST_ASSERT_TRUE(counter > 0);
    
    // 清理
    TEST_ASSERT_EQUALS(ppdb_base_timer_destroy(timer), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_async_loop_destroy(loop), PPDB_OK);
    return 0;
}

// IO完成回调函数
static void io_callback(ppdb_base_async_handle_t* handle, ppdb_error_t status) {
    ppdb_base_future_t* future = handle->data;
    if (status == PPDB_OK) {
        ppdb_base_future_set_result(future, handle->io.buf, handle->io.size);
    } else {
        ppdb_base_future_set_error(future, status);
    }
}

// 测试异步IO功能
static int test_async_io(void) {
    ppdb_base_async_loop_t* loop = NULL;
    ppdb_base_async_handle_t* handle = NULL;
    ppdb_base_future_t* future = NULL;
    char buf[256];
    bool ready = false;
    size_t bytes_read = 0;
    
    // 创建事件循环
    TEST_ASSERT_EQUALS(ppdb_base_async_loop_create(&loop), PPDB_OK);
    
    // 创建IO句柄
    TEST_ASSERT_EQUALS(ppdb_base_async_handle_create(loop, STDIN_FILENO, &handle), PPDB_OK);
    
    // 创建Future对象
    TEST_ASSERT_EQUALS(ppdb_base_future_create(loop, &future), PPDB_OK);
    handle->data = future;
    
    // 启动异步读取
    TEST_ASSERT_EQUALS(ppdb_base_async_read(handle, buf, sizeof(buf), io_callback), PPDB_OK);
    
    // 运行事件循环
    TEST_ASSERT_EQUALS(ppdb_base_async_loop_run(loop, 100), PPDB_OK);
    
    // 检查Future状态
    TEST_ASSERT_EQUALS(ppdb_base_future_is_ready(future, &ready), PPDB_OK);
    if (ready) {
        TEST_ASSERT_EQUALS(ppdb_base_future_get_result(future, buf, sizeof(buf), &bytes_read), PPDB_OK);
        TEST_ASSERT_TRUE(bytes_read > 0);
    }
    
    // 清理
    TEST_ASSERT_EQUALS(ppdb_base_future_destroy(future), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_async_handle_destroy(handle), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_async_loop_destroy(loop), PPDB_OK);
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_async_loop_basic);
    TEST_RUN(test_async_timer);
    TEST_RUN(test_async_io);
    
    TEST_REPORT();
    return 0;
}