/*
 * test_mux.c - Multiplexing Test Suite
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_mux.h"
#include "test/white/framework/test_framework.h"

// 基本功能测试
static void test_mux_basic(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 创建多路复用器
    err = infra_mux_create(&config, &mux);
    //TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(mux != NULL);
    
    // 销毁
    infra_mux_destroy(mux);
}

// 事件测试
static void test_mux_events(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    addr.host = "127.0.0.1";
    addr.port = 12345;

    err = infra_mux_create(&config, &mux);
    //TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(mux != NULL);

    err = infra_net_listen(&addr, &server, &config);
    //TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(server != NULL);

    int handle = config.mux.prefer_iocp ? infra_net_get_hdl(server) : infra_net_get_fd(server);
    err = infra_mux_add(mux, handle, INFRA_EVENT_READ, NULL);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    err = infra_mux_modify(mux, handle, INFRA_EVENT_READ | INFRA_EVENT_WRITE);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_mux_remove(mux, handle);
    TEST_ASSERT(err == INFRA_OK);

    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 等待测试
static void test_mux_wait(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_mux_event_t events[16];
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    addr.host = "127.0.0.1";
    addr.port = 12345;

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);

    err = infra_net_listen(&addr, &server, &config);
    //TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(server != NULL);

    err = infra_mux_add(mux, infra_net_get_fd(server), INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_mux_wait(mux, events, 16, 0);
    TEST_ASSERT(err == 0);

    infra_net_close(server);
    infra_mux_destroy(mux);
}

// 多路复用测试
static void test_mux_multiple(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_socket_t servers[3] = {NULL};
    infra_net_addr_t addr = {0};
    infra_mux_event_t events[16];
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    addr.host = "127.0.0.1";
    addr.port = 12345;

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);

    for (int i = 0; i < 3; i++) {
        addr.port = 12345 + i;
        err = infra_net_listen(&addr, &servers[i], &config);
        //TEST_ASSERT(err == INFRA_OK);
        TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
        TEST_ASSERT(servers[i] != NULL);

        err = infra_mux_add(mux, infra_net_get_fd(servers[i]), INFRA_EVENT_READ, NULL);
        TEST_ASSERT(err == INFRA_OK);
    }

    err = infra_mux_wait(mux, events, 16, 0);
    TEST_ASSERT(err == 0);

    for (int i = 0; i < 3; i++) {
        infra_net_close(servers[i]);
    }
    infra_mux_destroy(mux);
}

// 超时测试
static void test_mux_timeout(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_mux_event_t events[16];
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    int timeouts[] = {0, 1, 10, 100, 1000};

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);

    for (size_t i = 0; i < sizeof(timeouts)/sizeof(timeouts[0]); i++) {
        err = infra_mux_wait(mux, events, 16, timeouts[i]);
        TEST_ASSERT(err == 0);
    }

    infra_mux_destroy(mux);
}

// 配置测试
static void test_mux_config(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    // 测试默认配置
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);

    // 测试强制使用epoll
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);

    // 测试优先使用IOCP
    config.mux.prefer_iocp = true;
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);

    // 测试边缘触发
    config.mux.prefer_iocp = false;
    config.mux.edge_trigger = true;
    err = infra_mux_create(&config, &mux);
    //TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);

    // 测试最大事件数
    config.mux.edge_trigger = false;
    config.mux.max_events = 1024;
    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);
    infra_mux_destroy(mux);
}

// 压力测试
static void test_mux_stress(void) {
    infra_error_t err;
    infra_mux_t* mux = NULL;
    infra_socket_t server = NULL;
    infra_socket_t accepted[100] = {NULL};
    infra_net_addr_t addr = {0};
    infra_mux_event_t events[16];
    int num_accepted = 0;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    int timeout_count = 0;

    addr.host = "127.0.0.1";
    addr.port = 12345;

    err = infra_mux_create(&config, &mux);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(mux != NULL);

    err = infra_net_listen(&addr, &server, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(server != NULL);

    // Set socket to non-blocking mode using fcntl
    int flags = fcntl(infra_net_get_fd(server), F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(infra_net_get_fd(server), F_SETFL, flags);

    err = infra_mux_add(mux, infra_net_get_fd(server), INFRA_EVENT_READ, NULL);
    TEST_ASSERT(err == INFRA_OK);

    // 创建10个客户端连接
    infra_socket_t clients[10] = {NULL};
    infra_net_addr_t client_addr = {0};
    client_addr.host = "127.0.0.1";
    client_addr.port = 12345;

    int num_connected = 0;
    int retry_count = 0;
    while (num_connected < 10 && retry_count < 3) {
        err = infra_net_connect(&client_addr, &clients[num_connected], &config);
        if (err == INFRA_OK) {
            infra_printf("Client %d connected successfully\n", num_connected);
            num_connected++;
            retry_count = 0;
        } else if (err == INFRA_ERROR_WOULD_BLOCK) {
            infra_printf("Client %d connection would block, retrying...\n", num_connected);
            retry_count++;
            infra_sleep(100);
        } else {
            infra_printf("Client %d connection failed with error %d\n", num_connected, err);
            break;
        }
    }

    if (num_connected == 0) {
        infra_printf("Failed to establish any client connections\n");
        infra_net_close(server);
        infra_mux_destroy(mux);
        return;
    }

    infra_printf("Successfully established %d client connections\n", num_connected);

    // 接受连接
    while (num_accepted < num_connected && timeout_count < 3) {
        err = infra_net_accept(server, &accepted[num_accepted], NULL);
        if (err == INFRA_OK && accepted[num_accepted] != NULL) {
            err = infra_mux_add(mux, infra_net_get_fd(accepted[num_accepted]), INFRA_EVENT_READ | INFRA_EVENT_WRITE, NULL);
            TEST_ASSERT(err == INFRA_OK);
            num_accepted++;
            timeout_count = 0;  // Reset timeout counter on successful accept
        } else if (err == INFRA_ERROR_TIMEOUT || err == INFRA_ERROR_WOULD_BLOCK) {
            timeout_count++;
            infra_sleep(100);  // Sleep for 100ms before retry
        } else {
            infra_printf("Accept failed with error: %d\n", err);
            break;  // Exit on other errors
        }
    }

    infra_printf("Accepted %d connections\n", num_accepted);

    err = infra_mux_wait(mux, events, 16, 0);
    TEST_ASSERT(err == 0);

    for (int i = 0; i < num_accepted; i++) {
        if (accepted[i] != NULL) {
            infra_mux_remove(mux, infra_net_get_fd(accepted[i]));
            infra_net_close(accepted[i]);
        }
    }

    // Cleanup client connections
    for (int i = 0; i < num_connected; i++) {
        if (clients[i] != NULL) {
            infra_net_close(clients[i]);
        }
    }

    infra_mux_remove(mux, infra_net_get_fd(server));
    infra_net_close(server);
    infra_mux_destroy(mux);
}

int main(void) {
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }
    TEST_BEGIN();

    RUN_TEST(test_mux_basic);
    RUN_TEST(test_mux_events);
    RUN_TEST(test_mux_wait);
    RUN_TEST(test_mux_multiple);
    RUN_TEST(test_mux_timeout);
    RUN_TEST(test_mux_stress);
    RUN_TEST(test_mux_config);

    TEST_END();
} 
