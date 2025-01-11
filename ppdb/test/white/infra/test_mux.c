/*
 * test_mux.c - Multiplexing Infrastructure Layer Test
 */

#include "cosmopolitan.h"
#include "test/white/framework/test_framework.h"
#include "internal/infra/infra_mux.h"
#include "internal/infra/infra_net.h"

// 测试状态
static struct {
    bool callback_called;
    infra_event_type_t last_event;
    int event_count;
} g_test_state;

// 测试回调函数
static void test_event_callback(infra_mux_t* mux, int fd, infra_event_type_t event, void* user_data) {
    g_test_state.callback_called = true;
    g_test_state.last_event = event;
    g_test_state.event_count++;
}

// 基本功能测试
static void test_mux_basic(void) {
    infra_mux_t* mux = NULL;
    infra_error_t err;
    
    // 创建多路复用器
    err = infra_mux_create(&mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    
    // 测试设置
    err = infra_mux_set_capacity(mux, 1024);
    TEST_ASSERT(err == INFRA_OK);
    
    // 清理
    infra_mux_destroy(mux);
}

// 事件注册测试
static void test_mux_events(void) {
    infra_mux_t* mux = NULL;
    infra_net_addr_t addr = {"localhost", 12345};
    infra_socket_t server = NULL;
    infra_error_t err;
    
    // 初始化
    err = infra_mux_create(&mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建服务器
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    // 注册事件
    err = infra_mux_add(mux, server, INFRA_EVENT_READ, test_event_callback, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 修改事件
    err = infra_mux_mod(mux, server, INFRA_EVENT_READ | INFRA_EVENT_WRITE, test_event_callback, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 删除事件
    err = infra_mux_del(mux, server);
    TEST_ASSERT(err == INFRA_OK);
    
    // 清理
    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 事件循环测试
static void test_mux_loop(void) {
    infra_mux_t* mux = NULL;
    infra_net_addr_t addr = {"localhost", 12345};
    infra_socket_t server = NULL;
    infra_error_t err;
    
    // 初始化
    g_test_state.callback_called = false;
    g_test_state.event_count = 0;
    
    err = infra_mux_create(&mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建服务器
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    // 注册事件
    err = infra_mux_add(mux, server, INFRA_EVENT_READ, test_event_callback, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 运行事件循环（非阻塞）
    err = infra_mux_poll(mux, 100);  // 100ms超时
    TEST_ASSERT(err == INFRA_OK);
    
    // 清理
    infra_net_close(server);
    infra_mux_destroy(mux);
}

int main(void) {
    TEST_BEGIN();
    RUN_TEST(test_mux_basic);
    RUN_TEST(test_mux_events);
    RUN_TEST(test_mux_loop);
    TEST_END();
} 