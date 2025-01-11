/*
 * test_async.c - Async Infrastructure Layer Test
 */

#include "test/white/framework/test_framework.h"
#include "internal/infra/infra_async.h"

// 测试状态
static struct {
    bool callback_called;
    infra_event_type_t last_event;
    infra_buffer_t buffer;
} g_test_state;

// 测试回调函数
static void test_event_callback(int fd, infra_event_type_t event, void* user_data) {
    g_test_state.callback_called = true;
    g_test_state.last_event = event;
}

static infra_io_status_t test_read_callback(int fd, infra_buffer_t* buf, void* user_data) {
    return INFRA_IO_COMPLETE;
}

static infra_io_status_t test_write_callback(int fd, infra_buffer_t* buf, void* user_data) {
    return INFRA_IO_COMPLETE;
}

// 测试初始化和清理
static void test_setup(void) {
    g_test_state.callback_called = false;
    g_test_state.last_event = 0;
    infra_buffer_init(&g_test_state.buffer, 1024);
}

static void test_teardown(void) {
    infra_buffer_cleanup(&g_test_state.buffer);
}

// 测试缓冲区操作
void test_buffer_operations(void) {
    printf("\nRunning test: test_buffer_operations\n");
    
    infra_buffer_t buf;
    infra_error_t err;
    
    // 测试初始化
    err = infra_buffer_init(&buf, 16);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(buf.capacity == 16);
    TEST_ASSERT(buf.size == 0);
    
    // 测试追加数据
    const char* data = "Hello";
    err = infra_buffer_append(&buf, data, 5);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(buf.size == 5);
    TEST_ASSERT(memcmp(buf.data, data, 5) == 0);
    
    // 测试自动扩容
    err = infra_buffer_append(&buf, data, 5);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(buf.size == 10);
    TEST_ASSERT(buf.capacity >= 10);
    
    // 测试消费数据
    err = infra_buffer_consume(&buf, 5);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(buf.size == 5);
    TEST_ASSERT(memcmp(buf.data, data, 5) == 0);
    
    // 清理
    infra_buffer_cleanup(&buf);
}

// 测试异步初始化
void test_async_init(void) {
    printf("\nRunning test: test_async_init\n");
    
    // 测试初始化
    infra_error_t err = infra_async_init();
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试重复初始化
    err = infra_async_init();
    TEST_ASSERT(err == INFRA_ERROR_EXISTS);
    
    // 测试清理
    err = infra_async_cleanup();
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试重复清理
    err = infra_async_cleanup();
    TEST_ASSERT(err == INFRA_ERROR_NOT_INIT);
}

// 测试事件处理
void test_async_events(void) {
    printf("\nRunning test: test_async_events\n");
    
    infra_error_t err;
    
    // 初始化
    err = infra_async_init();
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建测试socket
    int fds[2];
    err = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    TEST_ASSERT(err == 0);
    
    // 添加事件监听
    err = infra_async_add(fds[0], 
                         INFRA_EVENT_READ,
                         test_event_callback,
                         test_read_callback,
                         test_write_callback,
                         NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // TODO: 添加更多事件测试
    
    // 清理
    close(fds[0]);
    close(fds[1]);
    infra_async_cleanup();
}

// 测试套件入口
int run_async_test_suite(void) {
    test_init();
    
    // 运行测试
    RUN_TEST(test_buffer_operations);
    RUN_TEST(test_async_init);
    RUN_TEST(test_async_events);
    
    // 输出结果
    test_report();
    test_cleanup();
    
    return g_test_stats[TEST_STATS_FAILED] ? 1 : 0;
} 