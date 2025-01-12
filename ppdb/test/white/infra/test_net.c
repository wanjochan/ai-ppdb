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

// 测试函数声明
static void test_net_basic(bool nonblocking);
static void test_net_connect(bool nonblocking);
static void test_net_transfer(bool nonblocking);
static void test_net_addr(void);
static void test_net_udp(void);
static void test_net_timeout(bool nonblocking);
static void test_net_concurrent(bool nonblocking);
static void test_net_large_data(bool nonblocking);

// 主测试函数
int main(void) {
    TEST_BEGIN();

    // 初始化 infra 系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    // 运行基本功能测试
    RUN_TEST_MODE(test_net_basic, false);
    RUN_TEST_MODE(test_net_basic, true);
    
    // 运行连接测试
    RUN_TEST_MODE(test_net_connect, false);
    RUN_TEST_MODE(test_net_connect, true);
    
    // 运行数据传输测试
    RUN_TEST_MODE(test_net_transfer, false);
    RUN_TEST_MODE(test_net_transfer, true);
    
    // 运行地址解析测试
    RUN_TEST(test_net_addr);
    
    // 运行UDP测试
    RUN_TEST(test_net_udp);
    
    // 运行超时测试
    RUN_TEST_MODE(test_net_timeout, false);
    RUN_TEST_MODE(test_net_timeout, true);
    
    // 运行并发测试
    RUN_TEST_MODE(test_net_concurrent, false);
    RUN_TEST_MODE(test_net_concurrent, true);
    
    // 运行大数据传输测试
    RUN_TEST_MODE(test_net_large_data, false);
    RUN_TEST_MODE(test_net_large_data, true);

    // 清理 infra 系统
    infra_cleanup();

    TEST_END();
}

// 辅助函数 - 运行阻塞和非阻塞测试
#define RUN_TEST_BOTH_MODES(test_func) do { \
    infra_printf("\nRunning %s in blocking mode:\n", #test_func); \
    test_func(false); \
    infra_sleep(200); \
    infra_printf("\nRunning %s in non-blocking mode:\n", #test_func); \
    test_func(true); \
    infra_sleep(200); \
} while(0)

// 基本功能测试
static void test_net_basic(bool nonblocking) {
    infra_error_t err;
    infra_socket_t server = NULL;
    infra_net_addr_t addr = {0};
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    addr.host = "127.0.0.1";
    addr.port = 12345;

    err = infra_net_listen(&addr, &server, &config);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(server != NULL);
    
    // 设置端口复用
    err = infra_net_set_reuseaddr(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    if (nonblocking) {
        // 测试设置非阻塞
        err = infra_net_set_nonblock(server, true);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 测试设置选项
    err = infra_net_set_keepalive(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试关闭
    err = infra_net_close(server);
    TEST_ASSERT(err == INFRA_OK);
    infra_sleep(200); // 增加等待时间
}

// 连接测试
static void test_net_connect(bool nonblocking) {
    infra_error_t err;
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_net_addr_t addr = {0};
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    int retry_count = 0;
    const int MAX_RETRIES = 50;
    int timeout_count = 0;
    const int MAX_TIMEOUTS = 3;

    addr.host = "127.0.0.1";
    addr.port = 12346;

    err = infra_net_listen(&addr, &server, &config);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(server != NULL);
    
    // 设置端口复用
    err = infra_net_set_reuseaddr(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    if (nonblocking) {
        // 设置服务器为非阻塞模式
        err = infra_net_set_nonblock(server, true);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 连接
    err = infra_net_connect(&addr, &client, &config);
    if (err == INFRA_ERROR_WOULD_BLOCK) {
        while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS) {
            infra_sleep(100);
            err = infra_net_connect(&addr, &client, &config);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                retry_count++;
                if (retry_count % 10 == 0) {
                    timeout_count++;
                    infra_printf("Connect timeout %d/3\n", timeout_count);
                }
            } else {
                break;
            }
        }
    }
    
    if (err != INFRA_OK) {
        infra_printf("Connect failed with error: %d after %d retries\n", err, retry_count);
    }
    
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(client != NULL);
    
    // 清理
    if (client) {
        infra_net_close(client);
        infra_sleep(100);
    }
    if (server) {
        infra_net_close(server);
        infra_sleep(100);
    }
}

// 数据传输测试
static void test_net_transfer(bool nonblocking) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_socket_t accepted = NULL;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    char send_buf[] = "Hello, World!";
    char recv_buf[64] = {0};
    size_t bytes;
    infra_error_t err;
    int retry_count = 0;
    const int MAX_RETRIES = 50;
    int timeout_count = 0;
    const int MAX_TIMEOUTS = 3;
    
    addr.host = "127.0.0.1";
    addr.port = 12347;
    
    err = infra_net_listen(&addr, &server, &config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 设置端口复用
    err = infra_net_set_reuseaddr(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    if (nonblocking) {
        // 设置服务器为非阻塞模式
        err = infra_net_set_nonblock(server, true);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 连接
    err = infra_net_connect(&addr, &client, &config);
    if (err == INFRA_ERROR_WOULD_BLOCK) {
        while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS) {
            infra_sleep(100);
            err = infra_net_connect(&addr, &client, &config);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                retry_count++;
                if (retry_count % 10 == 0) {
                    timeout_count++;
                    infra_printf("Connect timeout %d/3\n", timeout_count);
                }
            } else {
                break;
            }
        }
    }
    
    if (err != INFRA_OK) {
        infra_printf("Connect failed with error: %d after %d retries\n", err, retry_count);
    }
    
    TEST_ASSERT(err == INFRA_OK);
    
    // 接受连接
    retry_count = 0;
    timeout_count = 0;
    do {
        err = infra_net_accept(server, &accepted, &addr);
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            retry_count++;
            infra_sleep(100);
            if (retry_count % 10 == 0) {
                timeout_count++;
                infra_printf("Accept timeout %d/3\n", timeout_count);
            }
            continue;
        }
        break;
    } while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS);
    
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(accepted != NULL);
    
    // 发送数据
    retry_count = 0;
    timeout_count = 0;
    do {
        err = infra_net_send(client, send_buf, sizeof(send_buf), &bytes);
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            retry_count++;
            infra_sleep(100);
            if (retry_count % 10 == 0) {
                timeout_count++;
                infra_printf("Send timeout %d/3\n", timeout_count);
            }
            continue;
        }
        break;
    } while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS);
    
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    
    // 接收数据
    retry_count = 0;
    timeout_count = 0;
    do {
        err = infra_net_recv(accepted, recv_buf, sizeof(recv_buf), &bytes);
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            retry_count++;
            infra_sleep(100);
            if (retry_count % 10 == 0) {
                timeout_count++;
                infra_printf("Recv timeout %d/3\n", timeout_count);
            }
            continue;
        }
        break;
    } while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS);
    
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(bytes == sizeof(send_buf));
    TEST_ASSERT(infra_strcmp(send_buf, recv_buf) == 0);
    
    // 清理
    if (accepted) {
        infra_net_close(accepted);
        infra_sleep(100);
    }
    if (client) {
        infra_net_close(client);
        infra_sleep(100);
    }
    if (server) {
        infra_net_close(server);
        infra_sleep(100);
    }
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
    infra_error_t err;
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_net_addr_t addr = {0};
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    char send_buf[] = "Hello, UDP!";
    char recv_buf[64] = {0};
    size_t bytes;

    addr.host = "127.0.0.1";
    addr.port = 12345;
    
    // 创建UDP套接字
    err = infra_net_udp_bind(&addr, &server, &config);
    TEST_ASSERT(err == INFRA_OK);
    
    err = infra_net_udp_socket(&client, &config);
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
static void test_net_timeout(bool nonblocking) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    int retry_count = 0;
    const int MAX_RETRIES = 50;
    int timeout_count = 0;
    const int MAX_TIMEOUTS = 3;
    
    addr.host = "127.0.0.1";
    addr.port = 12348;
    
    err = infra_net_listen(&addr, &server, &config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 设置端口复用
    err = infra_net_set_reuseaddr(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    if (nonblocking) {
        // 设置服务器为非阻塞模式
        err = infra_net_set_nonblock(server, true);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 连接
    err = infra_net_connect(&addr, &client, &config);
    if (err == INFRA_ERROR_WOULD_BLOCK) {
        while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS) {
            infra_sleep(100);
            err = infra_net_connect(&addr, &client, &config);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                retry_count++;
                if (retry_count % 10 == 0) {
                    timeout_count++;
                    infra_printf("Connect timeout %d/3\n", timeout_count);
                }
            } else {
                break;
            }
        }
    }
    
    if (err != INFRA_OK) {
        infra_printf("Connect failed with error: %d after %d retries\n", err, retry_count);
    }
    
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
    if (client) {
        infra_net_close(client);
        infra_sleep(100);
    }
    if (server) {
        infra_net_close(server);
        infra_sleep(100);
    }
}

// 并发连接测试
static void test_net_concurrent(bool nonblocking) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t clients[100] = {0};
    infra_socket_t accepted[100] = {0};
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    
    addr.host = "127.0.0.1";
    addr.port = 12349;
    
    err = infra_net_listen(&addr, &server, &config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 设置端口复用
    err = infra_net_set_reuseaddr(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    if (nonblocking) {
        // 设置为非阻塞模式
        err = infra_net_set_nonblock(server, true);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 创建多个客户端连接
    for (int i = 0; i < 100; i++) {
        err = infra_net_connect(&addr, &clients[i], &config);
        if (nonblocking) {
            TEST_ASSERT(err == INFRA_OK || err == INFRA_ERROR_WOULD_BLOCK);
        } else {
            TEST_ASSERT(err == INFRA_OK);
        }
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            infra_sleep(10);
        }
    }
    
    // 接受所有连接
    int accepted_count = 0;
    int retry_count = 0;
    const int MAX_RETRIES = 1000; // 增加重试次数以处理大量连接
    
    while (accepted_count < 100 && retry_count < MAX_RETRIES) {
        err = infra_net_accept(server, &accepted[accepted_count], &addr);
        if (err == INFRA_OK) {
            accepted_count++;
        } else if (err == INFRA_ERROR_WOULD_BLOCK) {
            retry_count++;
            infra_sleep(10); // 减少睡眠时间以加快处理
        } else {
            TEST_ASSERT(0);  // 不应该出现其他错误
        }
    }
    
    TEST_ASSERT(accepted_count == 100);
    
    // 清理
    for (int i = 0; i < 100; i++) {
        if (clients[i]) {
            infra_net_close(clients[i]);
            infra_sleep(10);
        }
        if (accepted[i]) {
            infra_net_close(accepted[i]);
            infra_sleep(10);
        }
    }
    if (server) {
        infra_net_close(server);
        infra_sleep(100);
    }
}

// 大数据包测试
static void test_net_large_data(bool nonblocking) {
    infra_net_addr_t addr = {0};
    infra_socket_t server = NULL;
    infra_socket_t client = NULL;
    infra_socket_t accepted = NULL;
    char* large_buf;
    size_t buf_size = 1024 * 1024;  // 1MB数据
    size_t bytes;
    infra_error_t err;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    int retry_count = 0;
    const int MAX_RETRIES = 50;
    int timeout_count = 0;
    const int MAX_TIMEOUTS = 3;
    
    addr.host = "127.0.0.1";
    addr.port = 12350;
    
    // 分配大缓冲区
    large_buf = (char*)infra_malloc(buf_size);
    TEST_ASSERT(large_buf != NULL);
    
    // 填充测试数据
    for (size_t i = 0; i < buf_size; i++) {
        large_buf[i] = (char)(i & 0xFF);
    }
    
    // 创建连接
    err = infra_net_listen(&addr, &server, &config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 设置端口复用
    err = infra_net_set_reuseaddr(server, true);
    TEST_ASSERT(err == INFRA_OK);
    
    if (nonblocking) {
        // 设置服务器为非阻塞模式
        err = infra_net_set_nonblock(server, true);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 连接
    err = infra_net_connect(&addr, &client, &config);
    if (err == INFRA_ERROR_WOULD_BLOCK) {
        while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS) {
            infra_sleep(100);
            err = infra_net_connect(&addr, &client, &config);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                retry_count++;
                if (retry_count % 10 == 0) {
                    timeout_count++;
                    infra_printf("Connect timeout %d/3\n", timeout_count);
                }
            } else {
                break;
            }
        }
    }
    
    if (err != INFRA_OK) {
        infra_printf("Connect failed with error: %d after %d retries\n", err, retry_count);
    }
    
    TEST_ASSERT(err == INFRA_OK);
    
    // 接受连接
    retry_count = 0;
    timeout_count = 0;
    do {
        err = infra_net_accept(server, &accepted, &addr);
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            retry_count++;
            infra_sleep(100);
            if (retry_count % 10 == 0) {
                timeout_count++;
                infra_printf("Accept timeout %d/3\n", timeout_count);
            }
            continue;
        }
        break;
    } while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS);
    
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(accepted != NULL);
    
    // 发送大数据包
    size_t total_sent = 0;
    while (total_sent < buf_size) {
        retry_count = 0;
        timeout_count = 0;
        do {
            err = infra_net_send(client, large_buf + total_sent, 
                                buf_size - total_sent, &bytes);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                retry_count++;
                infra_sleep(100);
                if (retry_count % 10 == 0) {
                    timeout_count++;
                    infra_printf("Send timeout %d/3 at offset %zu\n", timeout_count, total_sent);
                }
                continue;
            }
            break;
        } while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS);
        
        TEST_ASSERT(err == INFRA_OK);
        total_sent += bytes;
        infra_printf("Sent %zu/%zu bytes\n", total_sent, buf_size);
    }
    
    // 接收大数据包
    char* recv_buf = (char*)infra_malloc(buf_size);
    TEST_ASSERT(recv_buf != NULL);
    
    size_t total_recv = 0;
    while (total_recv < buf_size) {
        retry_count = 0;
        timeout_count = 0;
        do {
            err = infra_net_recv(accepted, recv_buf + total_recv,
                                buf_size - total_recv, &bytes);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                retry_count++;
                infra_sleep(100);
                if (retry_count % 10 == 0) {
                    timeout_count++;
                    infra_printf("Recv timeout %d/3 at offset %zu\n", timeout_count, total_recv);
                }
                continue;
            }
            break;
        } while (retry_count < MAX_RETRIES && timeout_count < MAX_TIMEOUTS);
        
        TEST_ASSERT(err == INFRA_OK);
        total_recv += bytes;
        infra_printf("Received %zu/%zu bytes\n", total_recv, buf_size);
    }
    
    // 验证数据
    TEST_ASSERT(memcmp(large_buf, recv_buf, buf_size) == 0);
    
    // 清理
    infra_free(large_buf);
    infra_free(recv_buf);
    if (accepted) {
        infra_net_close(accepted);
        infra_sleep(100);
    }
    if (client) {
        infra_net_close(client);
        infra_sleep(100);
    }
    if (server) {
        infra_net_close(server);
        infra_sleep(100);
    }
} 