/*
 * test_mux.c - Multiplexing Test Suite
 */

#include "cosmopolitan.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_mux.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_sync.h"
#include "test/white/framework/test_framework.h"

// 事件回调计数器
static int g_event_count = 0;

// 基本功能测试
static void test_mux_basic(void) {
    infra_mux_t* mux = NULL;
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    
    // 销毁
    err = infra_mux_destroy(mux);
    TEST_ASSERT(err == INFRA_OK);
}

// 事件测试
static void test_mux_events(void) {
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建服务器
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 添加监听套接字
    err = infra_mux_add(mux, server, INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 修改事件
    err = infra_mux_modify(mux, server, INFRA_EVENT_READ | INFRA_EVENT_WRITE);
    TEST_ASSERT(err == INFRA_OK);
    
    // 移除套接字
    err = infra_mux_remove(mux, server);
    TEST_ASSERT(err == INFRA_OK);
    
    // 清理
    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 等待测试
static void test_mux_wait(void) {
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_socket_t accepted = NULL;
    infra_net_addr_t addr = {0};
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建服务器和客户端
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_set_nonblock(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_connect(&addr, &client);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_set_nonblock(client, true);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 添加套接字
    err = infra_mux_add(mux, server, INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 等待事件
    infra_mux_event_t events[10] = {0};
    err = infra_mux_wait(mux, events, 10, 100);  // 100ms超时
    TEST_ASSERT(err == INFRA_OK);
    
    // 接受连接
    err = infra_net_accept(server, &accepted, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 清理
    infra_net_close(accepted);
    infra_net_close(client);
    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 多路复用测试
static void test_mux_multiple(void) {
    infra_mux_t* mux = NULL;
    infra_socket_t servers[10] = {0};
    infra_socket_t clients[10] = {0};
    infra_net_addr_t addr = {0};
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建多个服务器和客户端
    for (int i = 0; i < 10; i++) {
        addr.port = 12345 + i;
        err = infra_net_listen(&addr, &servers[i]);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_net_set_nonblock(servers[i], true);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_mux_add(mux, servers[i], INFRA_EVENT_READ, NULL);
        TEST_ASSERT(err == INFRA_OK);

        err = infra_net_connect(&addr, &clients[i]);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_net_set_nonblock(clients[i], true);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 等待事件
    infra_mux_event_t events[10];
    err = infra_mux_wait(mux, events, 10, 100);  // 100ms超时
    TEST_ASSERT(err == INFRA_OK);
    
    // 清理
    for (int i = 0; i < 10; i++) {
        infra_net_close(clients[i]);
        infra_net_close(servers[i]);
    }
    infra_mux_destroy(mux);
}

// 超时测试
static void test_mux_timeout(void) {
    infra_mux_t* mux = NULL;
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试不同的超时值
    int timeouts[] = {0, 1, 10, 100, 1000};
    infra_mux_event_t events[10];
    
    for (int i = 0; i < sizeof(timeouts)/sizeof(timeouts[0]); i++) {
        uint64_t start = infra_time_ms();
        err = infra_mux_wait(mux, events, 10, timeouts[i]);
        uint64_t end = infra_time_ms();
        
        TEST_ASSERT(err == INFRA_OK);
        TEST_ASSERT(end - start >= (uint64_t)timeouts[i]);
        TEST_ASSERT(end - start < (uint64_t)(timeouts[i] + 100));  // 允许100ms误差
    }
    
    // 清理
    infra_mux_destroy(mux);
}

// 配置测试
static void test_mux_config(void) {
    infra_mux_t* mux = NULL;
    infra_error_t err;
    
    // 测试默认配置
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    infra_mux_destroy(mux);
    
    // 测试强制使用IOCP
    config.mux.force_iocp = true;
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    infra_mux_destroy(mux);
    
    // 测试强制使用epoll
    config = INFRA_DEFAULT_CONFIG;
    config.mux.force_epoll = true;
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    infra_mux_destroy(mux);
    
    // 测试边缘触发
    config = INFRA_DEFAULT_CONFIG;
    config.mux.edge_trigger = true;
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    infra_mux_destroy(mux);
    
    // 测试最大事件数
    config = INFRA_DEFAULT_CONFIG;
    config.mux.max_events = 2048;
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    infra_mux_destroy(mux);
}

// 压力测试
static void test_mux_stress(void) {
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_socket_t clients[100] = {0};
    infra_socket_t accepted[100] = {0};
    infra_net_addr_t addr = {0};
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建服务器
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_set_nonblock(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_mux_add(mux, server, INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建多个客户端
    for (int i = 0; i < 100; i++) {
        err = infra_net_connect(&addr, &clients[i]);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_net_set_nonblock(clients[i], true);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 等待并接受所有连接
    for (int i = 0; i < 100; i++) {
        infra_mux_event_t events[10];
        err = infra_mux_wait(mux, events, 10, 100);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_net_accept(server, &accepted[i], NULL);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_net_set_nonblock(accepted[i], true);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_mux_add(mux, accepted[i], INFRA_EVENT_READ | INFRA_EVENT_WRITE, NULL);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 清理
    for (int i = 0; i < 100; i++) {
        if (accepted[i]) {
            infra_mux_remove(mux, accepted[i]);
            infra_net_close(accepted[i]);
        }
        if (clients[i]) {
            infra_net_close(clients[i]);
        }
    }
    
    infra_mux_remove(mux, server);
    infra_net_close(server);
    infra_mux_destroy(mux);
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_mux_basic);
    TEST_RUN(test_mux_events);
    TEST_RUN(test_mux_wait);
    TEST_RUN(test_mux_multiple);
    TEST_RUN(test_mux_timeout);
    TEST_RUN(test_mux_config);
    TEST_RUN(test_mux_stress);
    
    TEST_CLEANUP();
    return 0;
} 