/*
 * test_net.c - Network Operations Test Suite
 */

#include "cosmopolitan.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_sync.h"
#include "test/white/framework/test_framework.h"

// 基本功能测试
static void test_net_basic(void) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_error_t err;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 测试监听
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(server != NULL);
    
    // 测试设置选项
    err = infra_net_set_nonblock(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_set_reuseaddr(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_set_keepalive(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试关闭
    err = infra_net_close(server);
    TEST_ASSERT(err == INFRA_OK);
}

// 连接测试
static void test_net_connect(void) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_error_t err;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建服务器
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试连接
    err = infra_net_connect(&addr, &client);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(client != NULL);
    
    // 测试非阻塞连接
    infra_socket_t async_client = NULL;
    err = infra_net_set_nonblock(client, true);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_connect(&addr, &async_client);
    TEST_ASSERT(err == INFRA_ERROR_WOULD_BLOCK || err == INFRA_OK);
    
    // 清理
    if (async_client) {
        infra_net_close(async_client);
    }
    infra_net_close(client);
    infra_net_close(server);
}

// 数据传输测试
static void test_net_transfer(void) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_socket_t accepted = NULL;
    char send_buf[] = "Hello, World!";
    char recv_buf[64] = {0};
    size_t bytes;
    infra_error_t err;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 测试监听
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_connect(&addr, &client);
    TEST_ASSERT(err == INFRA_OK);
    
    // 接受连接
    err = infra_net_accept(server, &accepted, &addr);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(accepted != NULL);
    
    // 发送数据
    err = infra_net_send(client, send_buf, sizeof(send_buf), &bytes);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    
    // 接收数据
    err = infra_net_recv(accepted, recv_buf, sizeof(recv_buf), &bytes);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    TEST_ASSERT(infra_strcmp(send_buf, recv_buf) == 0);
    
    // 清理
    infra_net_close(accepted);
    infra_net_close(client);
    infra_net_close(server);
}

// 地址解析测试
static void test_net_addr(void) {
    infra_net_addr_t addr = {0};
    infra_error_t err;
    
    // 测试解析主机名
    err = infra_net_resolve("localhost", &addr);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试解析IP地址
    err = infra_net_resolve("127.0.0.1", &addr);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试无效地址
    err = infra_net_resolve("invalid.host.name", &addr);
    TEST_ASSERT(err != INFRA_OK);
}

// UDP测试
static void test_net_udp(void) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    char send_buf[] = "Hello, UDP!";
    char recv_buf[64] = {0};
    size_t bytes;
    infra_error_t err;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建UDP套接字
    err = infra_net_udp_bind(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_udp_socket(&client);
    TEST_ASSERT(err == INFRA_OK);
    
    // 发送数据
    err = infra_net_sendto(client, send_buf, sizeof(send_buf), &addr, &bytes);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    
    // 接收数据
    err = infra_net_recvfrom(server, recv_buf, sizeof(recv_buf), &addr, &bytes);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    TEST_ASSERT(infra_strcmp(send_buf, recv_buf) == 0);
    
    // 清理
    infra_net_close(client);
    infra_net_close(server);
}

// 连接超时测试
static void test_net_timeout(void) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_error_t err;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建服务器
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建客户端
    err = infra_net_connect(&addr, &client);
    TEST_ASSERT(err == INFRA_OK);
    
    // 设置接收超时
    err = infra_net_set_timeout(client, 1000);  // 1秒超时
    TEST_ASSERT(err == INFRA_OK);
    
    // 尝试接收数据应该超时
    char buf[1];
    size_t received;
    err = infra_net_recv(client, buf, sizeof(buf), &received);
    TEST_ASSERT(err == INFRA_ERROR_TIMEOUT);
    
    // 清理
    infra_net_close(client);
    infra_net_close(server);
}

// 并发连接测试
static void test_net_concurrent(void) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t clients[100] = {0};
    infra_socket_t accepted[100] = {0};
    infra_error_t err;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建服务器
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    // 设置为非阻塞模式
    err = infra_net_set_nonblock(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建多个客户端连接
    for (int i = 0; i < 100; i++) {
        err = infra_net_connect(&addr, &clients[i]);
        TEST_ASSERT(err == INFRA_OK || err == INFRA_ERROR_WOULD_BLOCK);
    }
    
    // 接受所有连接
    int accepted_count = 0;
    
    while (accepted_count < 100) {
        err = infra_net_accept(server, &accepted[accepted_count], &addr);
        if (err == INFRA_OK) {
            accepted_count++;
        } else if (err != INFRA_ERROR_WOULD_BLOCK) {
            TEST_ASSERT(0);  // 不应该出现其他错误
        }
    }
    
    // 清理
    for (int i = 0; i < 100; i++) {
        infra_net_close(clients[i]);
        infra_net_close(accepted[i]);
    }
    infra_net_close(server);
}

// 大数据包测试
static void test_net_large_data(void) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_socket_t accepted = NULL;
    char* large_buf;
    size_t buf_size = 1024 * 1024;  // 1MB数据
    size_t bytes;
    infra_error_t err;
    
    // 设置地址
    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 分配大缓冲区
    large_buf = (char*)infra_malloc(buf_size);
    TEST_ASSERT(large_buf != NULL);
    
    // 填充测试数据
    for (size_t i = 0; i < buf_size; i++) {
        large_buf[i] = (char)(i & 0xFF);
    }
    
    // 创建连接
    err = infra_net_listen(&addr, &server);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_connect(&addr, &client);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_accept(server, &accepted, &addr);
    TEST_ASSERT(err == INFRA_OK);
    
    // 发送大数据包
    size_t total_sent = 0;
    while (total_sent < buf_size) {
        err = infra_net_send(client, large_buf + total_sent, 
                            buf_size - total_sent, &bytes);
        TEST_ASSERT(err == INFRA_OK);
        total_sent += bytes;
    }
    
    // 接收大数据包
    char* recv_buf = (char*)infra_malloc(buf_size);
    TEST_ASSERT(recv_buf != NULL);
    
    size_t total_recv = 0;
    while (total_recv < buf_size) {
        err = infra_net_recv(accepted, recv_buf + total_recv,
                            buf_size - total_recv, &bytes);
        TEST_ASSERT(err == INFRA_OK);
        total_recv += bytes;
    }
    
    // 验证数据
    TEST_ASSERT(memcmp(large_buf, recv_buf, buf_size) == 0);
    
    // 清理
    infra_free(large_buf);
    infra_free(recv_buf);
    infra_net_close(accepted);
    infra_net_close(client);
    infra_net_close(server);
}

int main(void) {
    TEST_BEGIN();

    // 全局初始化
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    RUN_TEST(test_net_basic);
    err = infra_sleep(100);  // 等待100ms
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_net_connect);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_net_transfer);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_net_addr);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_net_udp);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_net_timeout);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_net_concurrent);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    RUN_TEST(test_net_large_data);
    err = infra_sleep(100);
    MAIN_ASSERT(err == INFRA_OK);

    // 全局清理
    infra_cleanup();

    TEST_END();
} 