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
    
    // 清理
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
    size_t sent = 0;
    size_t received = 0;
    
    // 创建服务器和客户端
    TEST_ASSERT(infra_net_listen(&addr, &server) == INFRA_OK);
    TEST_ASSERT(infra_net_connect(&addr, &client) == INFRA_OK);
    TEST_ASSERT(infra_net_accept(server, &accepted, NULL) == INFRA_OK);
    
    // 测试发送和接收
    TEST_ASSERT(infra_net_send(client, send_buf, strlen(send_buf), &sent) == INFRA_OK);
    TEST_ASSERT(sent == strlen(send_buf));
    
    TEST_ASSERT(infra_net_recv(accepted, recv_buf, sizeof(recv_buf), &received) == INFRA_OK);
    TEST_ASSERT(received == sent);
    TEST_ASSERT(strcmp(send_buf, recv_buf) == 0);
    
    // 清理
    TEST_ASSERT(infra_net_close(accepted) == INFRA_OK);
    TEST_ASSERT(infra_net_close(client) == INFRA_OK);
    TEST_ASSERT(infra_net_close(server) == INFRA_OK);
}

// 地址解析测试
static void test_net_addr(void) {
    infra_net_addr_t addr;
    char buf[64];
    
    // 测试DNS解析
    TEST_ASSERT(infra_net_resolve("localhost", &addr) == INFRA_OK);
    TEST_ASSERT(addr.port == 0);
    
    // 测试地址转换
    addr.port = 8080;
    TEST_ASSERT(infra_net_addr_to_str(&addr, buf, sizeof(buf)) == INFRA_OK);
    TEST_ASSERT(strstr(buf, "8080") != NULL);
}

int main(void) {
    RUN_TEST(test_net_basic);
    RUN_TEST(test_net_connect);
    RUN_TEST(test_net_transfer);
    RUN_TEST(test_net_addr);
    return 0;
} 