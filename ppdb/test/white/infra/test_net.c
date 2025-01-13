/*
 * test_net.c - Network Test Suite
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_net.h"
#include "test/white/framework/test_framework.h"

// 基本功能测试
static void test_net_basic(void) {
    infra_error_t err;
    infra_socket_t sock = NULL;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    // 创建TCP socket
    err = infra_net_create(&sock, false, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(sock != NULL);
    
    // 销毁
    infra_net_close(sock);

    // 创建UDP socket
    err = infra_net_create(&sock, true, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(sock != NULL);
    
    // 销毁
    infra_net_close(sock);
}

// TCP服务器测试
static void test_net_tcp_server(void) {
    infra_error_t err;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    addr.host = "127.0.0.1";
    addr.port = 12345;

    // 创建socket
    err = infra_net_create(&server, false, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(server != NULL);

    // 绑定地址
    err = infra_net_bind(server, &addr);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    // 开始监听
    err = infra_net_listen(server);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    infra_net_close(server);
}

// UDP服务器测试
static void test_net_udp_server(void) {
    infra_error_t err;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    addr.host = "127.0.0.1";
    addr.port = 12346;

    // 创建socket
    err = infra_net_create(&server, true, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(server != NULL);

    // 绑定地址
    err = infra_net_bind(server, &addr);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);

    // UDP不需要listen
    err = infra_net_listen(server);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_OPERATION);

    infra_net_close(server);
}

// TCP客户端测试
static void test_net_tcp_client(void) {
    infra_error_t err;
    infra_socket_t client = NULL;
    infra_net_addr_t addr = {0};
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    addr.host = "127.0.0.1";
    addr.port = 12347;

    // 连接（预期失败，因为没有服务器）
    err = infra_net_connect(&addr, &client, &config);
    TEST_ASSERT(err != INFRA_OK);
    TEST_ASSERT(client == NULL);  // 连接失败时 client 应该保持为 NULL
}

// 配置测试
static void test_net_config(void) {
    infra_error_t err;
    infra_socket_t sock = NULL;
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    // 创建socket
    err = infra_net_create(&sock, false, &config);
    TEST_ASSERT_MSG(err==INFRA_OK,"err(%d)!=INFRA_OK(%d)",err,INFRA_OK);
    TEST_ASSERT(sock != NULL);

    // 测试各种socket选项
    err = infra_net_set_nonblock(sock, true);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_net_set_keepalive(sock, true);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_net_set_reuseaddr(sock, true);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_net_set_nodelay(sock, true);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_net_set_timeout(sock, 1000);
    TEST_ASSERT(err == INFRA_OK);

    infra_net_close(sock);
}

int main(void) {
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }
    TEST_BEGIN();

    RUN_TEST(test_net_basic);
    RUN_TEST(test_net_tcp_server);
    RUN_TEST(test_net_udp_server);
    RUN_TEST(test_net_tcp_client);
    RUN_TEST(test_net_config);

    TEST_END();
} 