/*
 * test_net.c - Network Operations Test Suite
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "test/white/framework/test_framework.h"

// 基本功能测试
static void test_net_basic(void) {
    infra_net_addr_t addr = {"localhost", 12345};
    infra_socket_t server = NULL;
    
    // 测试监听
    TEST_ASSERT(infra_net_listen(&addr, &server) == INFRA_OK);
    TEST_ASSERT(server != NULL);
    
    // 测试设置选项
    TEST_ASSERT(infra_net_set_nonblock(server, true) == INFRA_OK);
    TEST_ASSERT(infra_net_set_reuseaddr(server, true) == INFRA_OK);
    TEST_ASSERT(infra_net_set_keepalive(server, true) == INFRA_OK);
    TEST_ASSERT(infra_net_set_nodelay(server, true) == INFRA_OK);
    
    // 测试关闭
    TEST_ASSERT(infra_net_close(server) == INFRA_OK);
}

// 连接测试
static void test_net_connect(void) {
    infra_net_addr_t addr = {"localhost", 12345};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    
    // 创建服务器
    TEST_ASSERT(infra_net_listen(&addr, &server) == INFRA_OK);
    
    // 测试连接
    TEST_ASSERT(infra_net_connect(&addr, &client) == INFRA_OK);
    TEST_ASSERT(client != NULL);
    
    // 测试非阻塞连接
    infra_socket_t async_client = NULL;
    TEST_ASSERT(infra_net_set_nonblock(client, true) == INFRA_OK);
    TEST_ASSERT(infra_net_connect(&addr, &async_client) == INFRA_ERROR_WOULD_BLOCK);
    
    // 清理
    if (async_client) {
        infra_net_close(async_client);
    }
    TEST_ASSERT(infra_net_close(client) == INFRA_OK);
    TEST_ASSERT(infra_net_close(server) == INFRA_OK);
}

// 数据传输测试
static void test_net_transfer(void) {
    infra_net_addr_t addr = {"localhost", 12345};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_socket_t accepted = NULL;
    char send_buf[] = "Hello, World!";
    char recv_buf[64] = {0};
    size_t bytes;
    
    // 创建服务器和客户端
    TEST_ASSERT(infra_net_listen(&addr, &server) == INFRA_OK);
    TEST_ASSERT(infra_net_connect(&addr, &client) == INFRA_OK);
    
    // 接受连接
    TEST_ASSERT(infra_net_accept(server, &accepted) == INFRA_OK);
    TEST_ASSERT(accepted != NULL);
    
    // 发送数据
    TEST_ASSERT(infra_net_send(client, send_buf, sizeof(send_buf), &bytes) == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    
    // 接收数据
    TEST_ASSERT(infra_net_recv(accepted, recv_buf, sizeof(recv_buf), &bytes) == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    TEST_ASSERT(infra_strcmp(send_buf, recv_buf) == 0);
    
    // 清理
    TEST_ASSERT(infra_net_close(accepted) == INFRA_OK);
    TEST_ASSERT(infra_net_close(client) == INFRA_OK);
    TEST_ASSERT(infra_net_close(server) == INFRA_OK);
}

// 地址解析测试
static void test_net_addr(void) {
    infra_net_addr_t addr;
    
    // 测试解析主机名
    TEST_ASSERT(infra_net_resolve("localhost", 80, &addr) == INFRA_OK);
    TEST_ASSERT(addr.port == 80);
    
    // 测试解析IP地址
    TEST_ASSERT(infra_net_resolve("127.0.0.1", 8080, &addr) == INFRA_OK);
    TEST_ASSERT(addr.port == 8080);
    
    // 测试无效地址
    TEST_ASSERT(infra_net_resolve("invalid.host.name", 80, &addr) != INFRA_OK);
}

// UDP测试
static void test_net_udp(void) {
    infra_net_addr_t addr = {"localhost", 12345};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    char send_buf[] = "Hello, UDP!";
    char recv_buf[64] = {0};
    size_t bytes;
    
    // 创建UDP套接字
    TEST_ASSERT(infra_net_udp_bind(&addr, &server) == INFRA_OK);
    TEST_ASSERT(infra_net_udp_socket(&client) == INFRA_OK);
    
    // 发送数据
    TEST_ASSERT(infra_net_sendto(client, send_buf, sizeof(send_buf), &addr, &bytes) == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    
    // 接收数据
    infra_net_addr_t peer_addr;
    TEST_ASSERT(infra_net_recvfrom(server, recv_buf, sizeof(recv_buf), &peer_addr, &bytes) == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    TEST_ASSERT(infra_strcmp(send_buf, recv_buf) == 0);
    
    // 清理
    TEST_ASSERT(infra_net_close(client) == INFRA_OK);
    TEST_ASSERT(infra_net_close(server) == INFRA_OK);
}

int main(void) {
    TEST_BEGIN();
    RUN_TEST(test_net_basic);
    RUN_TEST(test_net_connect);
    RUN_TEST(test_net_transfer);
    RUN_TEST(test_net_addr);
    RUN_TEST(test_net_udp);
    TEST_END();
} 