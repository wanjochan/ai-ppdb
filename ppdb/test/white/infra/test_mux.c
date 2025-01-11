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
    int active_count;
} g_test_state;

// 测试回调函数
static void test_event_callback(infra_mux_t* mux, int fd, infra_event_type_t event, void* user_data) {
    g_test_state.callback_called = true;
    g_test_state.last_event = event;
    g_test_state.event_count++;
    
    // 如果是读事件,模拟读取数据
    if (event & INFRA_EVENT_READ) {
        char buf[128];
        size_t bytes;
        infra_net_recv((infra_socket_t)(size_t)fd, buf, sizeof(buf), &bytes);
    }
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

// 多事件源测试
static void test_mux_multiple(void) {
    infra_mux_t* mux = NULL;
    infra_net_addr_t addr = {"localhost", 12345};
    infra_socket_t servers[10];  // 10个服务器
    infra_socket_t clients[10];  // 10个客户端
    infra_error_t err;
    int i;
    
    // 创建多路复用器
    err = infra_mux_create(&mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建多个服务器和客户端
    for (i = 0; i < 10; i++) {
        addr.port = 12345 + i;
        
        err = infra_net_listen(&addr, &servers[i]);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_mux_add(mux, servers[i], INFRA_EVENT_READ, test_event_callback, NULL);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_net_connect(&addr, &clients[i]);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 发送数据触发事件
    const char* test_data = "test";
    size_t bytes;
    for (i = 0; i < 10; i++) {
        err = infra_net_send(clients[i], test_data, 5, &bytes);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 等待所有事件触发
    g_test_state.event_count = 0;
    while (g_test_state.event_count < 10) {
        err = infra_mux_poll(mux, 100);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 清理
    for (i = 0; i < 10; i++) {
        infra_mux_del(mux, servers[i]);
        infra_net_close(servers[i]);
        infra_net_close(clients[i]);
    }
    infra_mux_destroy(mux);
}

// 超时测试
static void test_mux_timeout(void) {
    infra_mux_t* mux = NULL;
    infra_error_t err;
    uint64_t start, end;
    
    // 创建多路复用器
    err = infra_mux_create(&mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试不同的超时时间
    int timeouts[] = {0, 100, 500};  // 毫秒
    for (int i = 0; i < 3; i++) {
        start = infra_get_time_ms();
        err = infra_mux_poll(mux, timeouts[i]);
        end = infra_get_time_ms();
        
        TEST_ASSERT(err == INFRA_OK);
        TEST_ASSERT(end - start >= timeouts[i]);
        TEST_ASSERT(end - start < timeouts[i] + 100);  // 允许100ms误差
    }
    
    infra_mux_destroy(mux);
}

// 高负载测试
static void test_mux_stress(void) {
    infra_mux_t* mux = NULL;
    infra_net_addr_t addr = {"localhost", 12345};
    infra_socket_t server = NULL;
    infra_socket_t clients[1000];  // 1000个客户端
    infra_error_t err;
    int i;
    
    // 创建多路复用器
    err = infra_mux_create(&mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建服务器
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_mux_add(mux, server, INFRA_EVENT_READ, test_event_callback, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建大量客户端连接
    for (i = 0; i < 1000; i++) {
        err = infra_net_connect(&addr, &clients[i]);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_mux_add(mux, clients[i], INFRA_EVENT_READ | INFRA_EVENT_WRITE, 
                           test_event_callback, NULL);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 运行事件循环一段时间
    g_test_state.event_count = 0;
    for (i = 0; i < 10; i++) {
        err = infra_mux_poll(mux, 100);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 清理
    for (i = 0; i < 1000; i++) {
        infra_mux_del(mux, clients[i]);
        infra_net_close(clients[i]);
    }
    infra_mux_del(mux, server);
    infra_net_close(server);
    infra_mux_destroy(mux);
}

int main(void) {
    TEST_BEGIN();
    RUN_TEST(test_mux_basic);
    RUN_TEST(test_mux_events);
    RUN_TEST(test_mux_loop);
    RUN_TEST(test_mux_multiple);
    RUN_TEST(test_mux_timeout);
    RUN_TEST(test_mux_stress);
    TEST_END();
} 