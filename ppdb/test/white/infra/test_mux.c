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
    infra_mux_ctx_t* mux = NULL;
    infra_error_t err;
    infra_mux_config_t config = {
        .type = INFRA_MUX_AUTO,
        .max_events = 1024,
        .edge_trigger = false
    };
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    
    // 获取类型
    infra_mux_type_t type;
    err = infra_mux_get_type(mux, &type);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(type >= INFRA_MUX_AUTO && type <= INFRA_MUX_SELECT);
    
    // 销毁
    err = infra_mux_destroy(mux);
    TEST_ASSERT(err == INFRA_OK);
}

// 事件测试
static void test_mux_events(void) {
    infra_mux_ctx_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_error_t err;
    infra_mux_config_t config = {
        .type = INFRA_MUX_AUTO,
        .max_events = 1024,
        .edge_trigger = false
    };
    
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
    err = infra_mux_add(mux, infra_net_get_fd(server), INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 修改事件
    err = infra_mux_modify(mux, infra_net_get_fd(server), 
                          INFRA_EVENT_READ | INFRA_EVENT_WRITE);
    TEST_ASSERT(err == INFRA_OK);
    
    // 移除套接字
    err = infra_mux_remove(mux, infra_net_get_fd(server));
    TEST_ASSERT(err == INFRA_OK);
    
    // 清理
    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 等待测试
static void test_mux_wait(void) {
    infra_mux_ctx_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_socket_t accepted = NULL;
    infra_net_addr_t addr = {0};
    infra_error_t err;
    infra_mux_config_t config = {
        .type = INFRA_MUX_AUTO,  // 使用自动选择模式
        .max_events = 1024,
        .edge_trigger = false
    };
    
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
    err = infra_mux_add(mux, infra_net_get_fd(server), INFRA_EVENT_READ, NULL);
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
    infra_mux_ctx_t* mux = NULL;
    infra_socket_t servers[10] = {0};
    infra_socket_t clients[10] = {0};
    infra_net_addr_t addr = {0};
    infra_error_t err;
    infra_mux_config_t config = {
        .type = INFRA_MUX_AUTO,  // 使用自动选择模式
        .max_events = 1024,
        .edge_trigger = false
    };
    
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
        
        err = infra_mux_add(mux, infra_net_get_fd(servers[i]), 
                           INFRA_EVENT_READ, NULL);
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
    infra_mux_ctx_t* mux = NULL;
    infra_error_t err;
    infra_mux_config_t config = {
        .type = INFRA_MUX_AUTO,
        .max_events = 1024,
        .edge_trigger = false
    };
    
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

// 压力测试
static void test_mux_stress(void) {
    infra_mux_ctx_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_socket_t clients[100] = {0};
    infra_socket_t accepted[100] = {0};
    infra_net_addr_t addr = {0};
    infra_error_t err;
    infra_mux_config_t config = {
        .type = INFRA_MUX_AUTO,  // 使用自动选择模式
        .max_events = 1024,
        .edge_trigger = false
    };
    
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
    
    // 添加服务器到多路复用器
    err = infra_mux_add(mux, infra_net_get_fd(server), INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建多个客户端连接
    for (int i = 0; i < 100; i++) {
        err = infra_net_connect(&addr, &clients[i]);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_net_set_nonblock(clients[i], true);
        TEST_ASSERT(err == INFRA_OK);
        
        // 接受连接
        err = infra_net_accept(server, &accepted[i], NULL);
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            // 等待一会儿再试
            infra_sleep(1);
            err = infra_net_accept(server, &accepted[i], NULL);
        }
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_net_set_nonblock(accepted[i], true);
        TEST_ASSERT(err == INFRA_OK);
        
        // 添加到多路复用器
        err = infra_mux_add(mux, infra_net_get_fd(accepted[i]),
                           INFRA_EVENT_READ | INFRA_EVENT_WRITE, NULL);
        TEST_ASSERT(err == INFRA_OK);
        
        err = infra_mux_add(mux, infra_net_get_fd(clients[i]),
                           INFRA_EVENT_READ | INFRA_EVENT_WRITE, NULL);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 等待并处理事件
    infra_mux_event_t events[100];
    for (int i = 0; i < 10; i++) {
        err = infra_mux_wait(mux, events, 100, 100);  // 100ms超时
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 清理
    for (int i = 0; i < 100; i++) {
        infra_net_close(clients[i]);
        infra_net_close(accepted[i]);
    }
    infra_net_close(server);
    infra_mux_destroy(mux);
}

int main(void) {
    TEST_BEGIN();

    // 全局初始化
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    RUN_TEST(test_mux_basic);
    err = infra_sleep(100);  // 等待100ms
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_mux_events);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_mux_wait);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_mux_multiple);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_mux_timeout);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_mux_stress);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    // 全局清理
    infra_cleanup();

    TEST_END();
} 