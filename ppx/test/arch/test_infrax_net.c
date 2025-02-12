#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
// #include "internal/infrax/InfraxAsync.h"//todo later after polyxAsync is done
//#include "internal/polyx/PolyxAsync.h"
#include <string.h>  
#include <poll.h> //tmp testing

// 添加错误码定义
#define INFRAX_ERROR_SYNC_CREATE_FAILED -200
#define INFRAX_ERROR_CORE_INIT_FAILED -201
#define INFRAX_ERROR_NET_TIMEOUT INFRAX_ERROR_NET_WOULD_BLOCK_CODE
#define INFRAX_ERROR_INVALID_DATA -100  

// 添加 ASSERT 宏定义
#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            core->assert_failed(core, __FILE__, __LINE__, __func__, #expr, "Assertion failed"); \
            return; \
        } \
    } while (0)

// 测试参数定义
#define TEST_LARGE_DATA_SIZE (4 * 1024)  // 4KB total data
#define TEST_CHUNK_SIZE (1024)           // 1KB per chunk
#define TEST_TRANSFER_TIMEOUT_MS 5000    // 5 second timeout

// 全局变量
static InfraxCore* core = NULL;
static InfraxSync* test_mutex = NULL;
static InfraxSync* test_cond = NULL;
static InfraxThread* tcp_server_thread_handle = NULL;
static InfraxThread* udp_server_thread_handle = NULL;
static bool tcp_server_ready = false;
static bool tcp_server_running = false;
static bool udp_server_ready = false;
static bool udp_server_running = false;
static InfraxNetAddr tcp_server_addr = {0};
static InfraxNetAddr udp_server_addr = {0};

static InfraxError ensure_core_initialized() {
    if (core) {
        return INFRAX_ERROR_OK_STRUCT;
    }

    core = InfraxCoreClass.new();
    if (!core) {
        return (InfraxError){.code = INFRAX_ERROR_FAILED, .message = "Failed to create core"};
    }

    InfraxError err = infrax_net_addr_from_string("127.0.0.1", 12345, &tcp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        return err;
    }

    err = infrax_net_addr_from_string("127.0.0.1", 12346, &udp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        return err;
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static void* tcp_server_thread(void* arg) {
    (void)arg;

    // 创建服务器socket
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,  // 使用阻塞模式
        .send_timeout_ms = TEST_TRANSFER_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TRANSFER_TIMEOUT_MS,
        .reuse_addr = true
    };

    InfraxSocket* server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "Failed to create server socket\n");
        return NULL;
    }

    // 绑定地址
    InfraxError err = server->bind(server, &tcp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to bind: %s\n", err.message);
        InfraxSocketClass.free(server);
        return NULL;
    }

    // 开始监听
    err = server->listen(server, 5);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to listen: %s\n", err.message);
        InfraxSocketClass.free(server);
        return NULL;
    }

    // 通知主线程服务器已就绪
    test_mutex->mutex_lock(test_mutex);
    tcp_server_ready = true;
    test_cond->cond_signal(test_cond);
    test_mutex->mutex_unlock(test_mutex);

    while (tcp_server_running) {
        // 接受连接
        InfraxSocket* client;
        InfraxNetAddr client_addr;
        err = server->accept(server, &client, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (!tcp_server_running) break;  // 正常退出
            core->printf(core, "Accept failed: %s\n", err.message);
            continue;
        }

        // 处理客户端连接
        char buffer[TEST_CHUNK_SIZE];
        size_t total_received = 0;

        while (tcp_server_running) {
            size_t received;
            err = client->recv(client, buffer, sizeof(buffer), &received);
            
            if (INFRAX_ERROR_IS_ERR(err)) {
                if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK) {
                    core->sleep_ms(core, 10);  // 短暂等待后重试
                    continue;
                }
                core->printf(core, "Server receive error: %s\n", err.message);
                break;
            }

            if (received == 0) {
                core->printf(core, "Client disconnected\n");
                break;
            }

            total_received += received;
            core->printf(core, "Server received %zu bytes\n", received);

            // 回显数据
            size_t sent;
            err = client->send(client, buffer, received, &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Server send error: %s\n", err.message);
                break;
            }

            core->printf(core, "Server sent %zu bytes\n", sent);
            core->sleep_ms(core, 10);  // 短暂等待，避免发送过快
        }

        InfraxSocketClass.free(client);
    }

    InfraxSocketClass.free(server);
    return NULL;
}

static void* udp_server_thread(void* arg) {
    (void)arg;
    InfraxError last_error = INFRAX_ERROR_OK_STRUCT;

    // 创建服务器socket
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,  // 使用阻塞模式
        .send_timeout_ms = TEST_TRANSFER_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TRANSFER_TIMEOUT_MS,
        .reuse_addr = true
    };

    InfraxSocket* server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "Failed to create UDP server socket\n");
        return NULL;
    }

    // 绑定地址
    InfraxError err = server->bind(server, &udp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to bind UDP server: %s\n", err.message);
        InfraxSocketClass.free(server);
        return NULL;
    }

    // 通知主线程服务器已就绪
    test_mutex->mutex_lock(test_mutex);
    udp_server_ready = true;
    test_cond->cond_signal(test_cond);
    test_mutex->mutex_unlock(test_mutex);

    char buffer[TEST_CHUNK_SIZE];
    while (udp_server_running) {
        InfraxNetAddr client_addr;
        size_t received;
        err = server->recvfrom(server, buffer, sizeof(buffer), &received, &client_addr);
        
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_TIMEOUT) {
                continue;  // 超时，继续等待
            }
            core->printf(core, "UDP server receive error: %s\n", err.message);
            last_error = err;  // 记录最后一个错误
            continue;
        }

        if (received == 0) {
            continue;  // 没有数据
        }

        core->printf(core, "UDP server received %zu bytes\n", received);

        // 回显数据
        size_t sent;
        err = server->sendto(server, buffer, received, &sent, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "UDP server send error: %s\n", err.message);
            last_error = err;  // 记录最后一个错误
            continue;
        }

        core->printf(core, "UDP server sent %zu bytes\n", sent);
        core->sleep_ms(core, 10);  // 短暂等待，避免发送过快
    }

    InfraxSocketClass.free(server);
    
    // 如果有错误发生，返回 NULL
    if (INFRAX_ERROR_IS_ERR(last_error)) {
        return NULL;
    }
    
    return (void*)1;  // 成功退出
}

static void test_config() {
    core->printf(core, "Testing socket configuration...\n");
    
    InfraxError err = ensure_core_initialized();
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Core initialization failed: %s\n", err.message);
        return;
    }

    // Test TCP socket configuration
    InfraxSocketConfig tcp_config = {
        .is_udp = false,
        .is_nonblocking = false,
        .reuse_addr = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    InfraxSocket* tcp_socket = InfraxSocketClass.new(&tcp_config);
    if (!tcp_socket) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "tcp_socket != NULL", "Failed to create TCP socket");
        return;
    }

    // Test setting socket options
    int reuse = 1;
    err = tcp_socket->set_option(tcp_socket, INFRAX_SOL_SOCKET, INFRAX_SO_REUSEADDR, &reuse, sizeof(reuse));
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "set_option(SO_REUSEADDR) succeeded", "Failed to set socket option");
        goto cleanup_tcp;
    }

    int keep_alive = 1;
    err = tcp_socket->set_option(tcp_socket, INFRAX_SOL_SOCKET, INFRAX_SO_KEEPALIVE, &keep_alive, sizeof(keep_alive));
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "set_option(SO_KEEPALIVE) succeeded", "Failed to set socket option");
        goto cleanup_tcp;
    }

    // Test getting socket options
    int get_reuse = 0;
    size_t get_reuse_len = sizeof(get_reuse);
    err = tcp_socket->get_option(tcp_socket, INFRAX_SOL_SOCKET, INFRAX_SO_REUSEADDR, &get_reuse, &get_reuse_len);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to get SO_REUSEADDR option: %s\n", err.message);
        goto cleanup_tcp;
    }
    if (get_reuse == 0) {
        core->printf(core, "SO_REUSEADDR is not enabled\n");
        goto cleanup_tcp;
    }

    int get_keep_alive = 0;
    size_t get_keep_alive_len = sizeof(get_keep_alive);
    err = tcp_socket->get_option(tcp_socket, INFRAX_SOL_SOCKET, INFRAX_SO_KEEPALIVE, &get_keep_alive, &get_keep_alive_len);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to get SO_KEEPALIVE option: %s\n", err.message);
        goto cleanup_tcp;
    }
    if (get_keep_alive == 0) {
        core->printf(core, "SO_KEEPALIVE is not enabled\n");
        goto cleanup_tcp;
    }

    // Test UDP socket configuration
    InfraxSocketConfig udp_config = {
        .is_udp = true,
        .is_nonblocking = false,
        .reuse_addr = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    
    InfraxSocket* udp_socket = InfraxSocketClass.new(&udp_config);
    if (!udp_socket) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "udp_socket != NULL", "Failed to create UDP socket");
        goto cleanup_both;
    }

    // Test setting UDP socket options
    err = udp_socket->set_option(udp_socket, INFRAX_SOL_SOCKET, INFRAX_SO_REUSEADDR, &reuse, sizeof(reuse));
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to set SO_REUSEADDR for UDP: %s\n", err.message);
        goto cleanup_both;
    }

    // Test getting UDP socket options
    get_reuse = 0;
    get_reuse_len = sizeof(get_reuse);
    err = udp_socket->get_option(udp_socket, INFRAX_SOL_SOCKET, INFRAX_SO_REUSEADDR, &get_reuse, &get_reuse_len);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to get SO_REUSEADDR for UDP: %s\n", err.message);
        goto cleanup_both;
    }
    if (get_reuse == 0) {
        core->printf(core, "UDP SO_REUSEADDR is not enabled\n");
        goto cleanup_both;
    }

    // Test setting timeouts
    err = tcp_socket->set_timeout(tcp_socket, 2000, 2000);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to set TCP timeouts: %s\n", err.message);
        goto cleanup_both;
    }

    err = udp_socket->set_timeout(udp_socket, 2000, 2000);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to set UDP timeouts: %s\n", err.message);
        goto cleanup_both;
    }

    // Test setting non-blocking mode
    err = tcp_socket->set_nonblock(tcp_socket, true);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to set TCP non-blocking mode: %s\n", err.message);
        goto cleanup_both;
    }

    err = udp_socket->set_nonblock(udp_socket, true);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to set UDP non-blocking mode: %s\n", err.message);
        goto cleanup_both;
    }

    core->printf(core, "Socket configuration tests passed\n");

cleanup_both:
    InfraxSocketClass.free(udp_socket);
cleanup_tcp:
    InfraxSocketClass.free(tcp_socket);
}

static int test_tcp() {
    core->printf(core, "Testing TCP functionality...\n");
    
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 3000,
        .recv_timeout_ms = 3000,
        .reuse_addr = true
    };

    // 等待TCP服务器就绪
    InfraxError err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex: %s\n", err.message);
        return -1;
    }
    
    while (!tcp_server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait for condition: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            return -1;
        }
    }
    
    test_mutex->mutex_unlock(test_mutex);

    // 创建客户端socket
    InfraxSocket* socket = InfraxSocketClass.new(&config);
    if (!socket) {
        core->printf(core, "Failed to create socket\n");
        return -1;
    }

    // 连接到服务器
    err = socket->connect(socket, &tcp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to connect: %s\n", err.message);
        InfraxSocketClass.free(socket);
        return -1;
    }

    // 等待连接建立
    core->sleep_ms(core, 100);

    // 测试不同的数据模式
    const char* patterns[] = {
        "Hello, World!",
        "The quick brown fox jumps",
        "Pack my box with five dozen liquor jugs",
        "The five boxing wizards jump quickly pack my box with five dozen liquor jugs"
    };
    
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
        core->printf(core, "Testing pattern %zu...\n", i + 1);
        const char* pattern = patterns[i];
        size_t pattern_len = core->strlen(core, pattern);
        
        // 发送数据
        size_t total_sent = 0;
        while (total_sent < pattern_len) {
            size_t sent;
            err = socket->send(socket, pattern + total_sent, pattern_len - total_sent, &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to send pattern %zu: %s\n", i + 1, err.message);
                InfraxSocketClass.free(socket);
                return -1;
            }
            total_sent += sent;
        }
        
        // 等待服务器处理数据
        core->sleep_ms(core, 100);
        
        // 接收回显
        char recv_buf[256] = {0};
        size_t total_received = 0;
        
        while (total_received < pattern_len) {
            size_t received;
            err = socket->recv(socket, recv_buf + total_received, pattern_len - total_received, &received);
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to receive echo for pattern %zu: %s\n", i + 1, err.message);
                InfraxSocketClass.free(socket);
                return -1;
            }
            
            if (received == 0) {
                core->printf(core, "Connection closed by server\n");
                InfraxSocketClass.free(socket);
                return -1;
            }
            
            total_received += received;
            core->printf(core, "Server echoed %zu bytes\n", received);
            
            // 等待一小段时间让服务器准备下一个数据块
            core->sleep_ms(core, 10);
        }
        
        // 验证数据
        if (core->strncmp(core, pattern, recv_buf, pattern_len) != 0) {
            core->printf(core, "Data verification failed for pattern %zu\n", i + 1);
            InfraxSocketClass.free(socket);
            return -1;
        }
        
        core->printf(core, "Pattern %zu test passed\n", i + 1);
        
        // 等待一段时间再发送下一个模式
        core->sleep_ms(core, 100);
    }

    core->printf(core, "All TCP tests passed\n");
    InfraxSocketClass.free(socket);
    return 0;
}

static int test_udp() {
    core->printf(core, "Testing UDP functionality...\n");

    // 启动UDP服务器
    udp_server_ready = false;
    udp_server_running = true;

    // 创建UDP服务器线程
    InfraxThreadConfig thread_config = {
        .name = "udp_server",
        .func = udp_server_thread,
        .arg = NULL
    };

    udp_server_thread_handle = InfraxThreadClass.new(&thread_config);
    if (!udp_server_thread_handle) {
        core->printf(core, "Failed to create UDP server thread\n");
        return -1;
    }

    // 等待UDP服务器就绪
    test_mutex->mutex_lock(test_mutex);
    while (!udp_server_ready) {
        test_cond->cond_wait(test_cond, test_mutex);
    }
    test_mutex->mutex_unlock(test_mutex);

    // 创建UDP客户端
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TRANSFER_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TRANSFER_TIMEOUT_MS,
        .reuse_addr = false
    };

    InfraxSocket* client = InfraxSocketClass.new(&config);
    if (!client) {
        core->printf(core, "Failed to create UDP client\n");
        return -1;
    }

    // 准备测试数据
    const char* test_data = "UDP Test Message";
    size_t data_len = strlen(test_data);

    // 发送数据
    size_t sent;
    InfraxError err = client->sendto(client, test_data, data_len, &sent, &udp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to send UDP data: %s\n", err.message);
        InfraxSocketClass.free(client);
        return -1;
    }

    core->printf(core, "UDP client sent %zu bytes\n", sent);

    // 接收回显数据
    char recv_buffer[TEST_CHUNK_SIZE];
    size_t received;
    InfraxNetAddr server_addr;
    err = client->recvfrom(client, recv_buffer, sizeof(recv_buffer), &received, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to receive UDP data: %s\n", err.message);
        InfraxSocketClass.free(client);
        return -1;
    }

    core->printf(core, "UDP client received %zu bytes\n", received);

    // 验证数据
    if (received != data_len || memcmp(test_data, recv_buffer, data_len) != 0) {
        core->printf(core, "UDP data verification failed\n");
        InfraxSocketClass.free(client);
        return -1;
    }

    core->printf(core, "UDP test passed\n");

    // 清理资源
    InfraxSocketClass.free(client);
    udp_server_running = false;
    if (udp_server_thread_handle) {
        void* thread_result;
        err = udp_server_thread_handle->join(udp_server_thread_handle, &thread_result);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to join UDP server thread: %s\n", err.message);
        }
        InfraxThreadClass.free(udp_server_thread_handle);
        udp_server_thread_handle = NULL;
    }

    return 0;
}

// 压力测试
static int test_net_stress() {
    core->printf(core, "Running network stress test...\n");
    
    // 等待TCP服务器就绪
    InfraxError err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in stress test: %s\n", err.message);
        return -1;
    }
    
    while (!tcp_server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait for condition in stress test: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            return -1;
        }
    }
    
    test_mutex->mutex_unlock(test_mutex);
    
    #define STRESS_CLIENTS 5     
    #define STRESS_ITERATIONS 10  
    
    InfraxSocket* sockets[STRESS_CLIENTS] = {NULL};
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 1000,  
        .recv_timeout_ms = 1000,  
        .reuse_addr = true
    };
    
    // 创建并启动多个客户端
    for (int i = 0; i < STRESS_CLIENTS; i++) {
        sockets[i] = InfraxSocketClass.new(&config);
        if (!sockets[i]) {
            core->printf(core, "Failed to create socket for client %d\n", i);
            continue;
        }
        
        // 连接到服务器
        err = sockets[i]->connect(sockets[i], &tcp_server_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code != INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                core->printf(core, "Failed to connect client %d: %s\n", i, err.message);
            }
            continue;
        }
        
        // 发送和接收数据
        for (int j = 0; j < STRESS_ITERATIONS; j++) {
            char send_buf[64];
            core->snprintf(core, send_buf, sizeof(send_buf), "Client %d Message %d", i, j);
            size_t sent;
            err = sockets[i]->send(sockets[i], send_buf, core->strlen(core, send_buf), &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                if (err.code != INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                    core->printf(core, "Failed to send data from client %d: %s\n", i, err.message);
                }
                break;
            }
            
            // 等待一小段时间以确保数据发送完成
            core->sleep_ms(core, 10);
            
            char recv_buf[64];
            size_t received;
            err = sockets[i]->recv(sockets[i], recv_buf, sizeof(recv_buf), &received);
            if (INFRAX_ERROR_IS_ERR(err)) {
                if (err.code != INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                    core->printf(core, "Failed to receive data in client %d: %s\n", i, err.message);
                }
                break;
            }
            
            // 验证接收到的数据
            if (received > 0 && (received != sent || core->strncmp(core, recv_buf, send_buf, sent) != 0)) {
                core->printf(core, "Data mismatch for client %d iteration %d\n", i, j);
            }
            
            // 等待一小段时间再进行下一次迭代
            core->sleep_ms(core, 10);
        }
    }
    
    // 清理
    for (int i = 0; i < STRESS_CLIENTS; i++) {
        if (sockets[i]) {
            InfraxSocketClass.free(sockets[i]);
        }
    }
    
    core->printf(core, "Network stress test completed\n");
    return 0;
}

// 错误恢复测试
static int test_net_error_recovery() {
    core->printf(core, "Testing network error recovery...\n");
    
    // 等待TCP服务器就绪
    InfraxError err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in error recovery test: %s\n", err.message);
        return -1;
    }
    
    while (!tcp_server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait for condition in error recovery test: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            return -1;
        }
    }
    
    test_mutex->mutex_unlock(test_mutex);
    
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,  // 使用阻塞模式
        .reuse_addr = true,
        .send_timeout_ms = TEST_TRANSFER_TIMEOUT_MS,  // 使用定义的超时时间
        .recv_timeout_ms = TEST_TRANSFER_TIMEOUT_MS  // 使用定义的超时时间
    };
    
    InfraxSocket* socket = InfraxSocketClass.new(&config);
    if (!socket) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, 
            "socket != NULL", "Failed to create error recovery test socket");
        return -1;
    }
    
    // 测试连接到无效地址
    InfraxNetAddr invalid_addr;
    core->strcpy(core, invalid_addr.ip, "256.256.256.256");  
    invalid_addr.port = 12345;
    
    core->printf(core, "Testing connection to invalid address...\n");
    err = socket->connect(socket, &invalid_addr);
    if (!INFRAX_ERROR_IS_ERR(err)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, 
            "INFRAX_ERROR_IS_ERR(err)", "Connection to invalid address should fail");
        InfraxSocketClass.free(socket);
        return -1;
    } else {
        core->printf(core, "Expected error connecting to invalid address: %s\n", err.message);
    }
    
    // 测试重连机制
    int retry_count = 3;  // 最多重试3次
    bool connected = false;
    core->printf(core, "Testing reconnection mechanism...\n");
    
    while (retry_count-- > 0 && !connected) {
        core->printf(core, "Connection attempt %d...\n", 3 - retry_count);
        err = socket->connect(socket, &tcp_server_addr);
        if (!INFRAX_ERROR_IS_ERR(err)) {
            connected = true;
            core->printf(core, "Successfully connected on attempt %d\n", 3 - retry_count);
            break;
        }
        core->printf(core, "Retrying connection: %s\n", err.message);
        core->sleep_ms(core, 100);  // 等待100ms再重试
    }
    
    if (connected) {
        // 等待服务器准备就绪
        core->sleep_ms(core, 500);  // 等待500ms让服务器准备就绪
        
        // 发送一些数据
        const char* test_data = "Test error recovery";
        size_t data_len = core->strlen(core, test_data);
        size_t total_sent = 0;
        
        core->printf(core, "Sending test data...\n");
        while (total_sent < data_len) {
            size_t sent;
            err = socket->send(socket, test_data + total_sent, data_len - total_sent, &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to send data: %s\n", err.message);
                InfraxSocketClass.free(socket);
                return -1;
            }
            total_sent += sent;
            core->printf(core, "Sent %zu/%zu bytes\n", total_sent, data_len);
        }
        
        if (total_sent == data_len) {
            core->printf(core, "Successfully sent all %zu bytes\n", total_sent);
            
            // 等待服务器处理完所有数据
            core->sleep_ms(core, 100);

            // 接收回显数据
            char recv_buf[64] = {0};
            size_t total_received = 0;
            
            // 等待一段时间让服务器处理数据
            core->sleep_ms(core, 500);  // 等待500ms让服务器处理数据
            
            core->printf(core, "Waiting for echo response...\n");
            while (total_received < total_sent) {
                size_t received;
                err = socket->recv(socket, recv_buf + total_received, 
                                 sizeof(recv_buf) - total_received, &received);
                if (INFRAX_ERROR_IS_ERR(err)) {
                    core->printf(core, "Failed to receive data: %s\n", err.message);
                    InfraxSocketClass.free(socket);
                    return -1;
                }
                if (received == 0) {
                    core->printf(core, "Connection closed by peer\n");
                    InfraxSocketClass.free(socket);
                    return -1;
                }
                total_received += received;
                core->printf(core, "Received %zu/%zu bytes\n", total_received, total_sent);
            }
            
            if (total_received == total_sent) {
                core->printf(core, "Successfully received all %zu bytes\n", total_received);
                // 验证数据
                if (core->strncmp(core, recv_buf, test_data, total_received) != 0) {
                    core->printf(core, "Data verification failed!\n");
                    core->printf(core, "Expected: %.*s\n", (int)total_sent, test_data);
                    core->printf(core, "Received: %.*s\n", (int)total_received, recv_buf);
                    core->assert_failed(core, __FILE__, __LINE__, __func__, 
                        "Data verification", "Received data does not match sent data");
                    InfraxSocketClass.free(socket);
                    return -1;
                } else {
                    core->printf(core, "Data verification successful\n");
                }
            } else {
                core->printf(core, "Incomplete receive: got %zu of %zu bytes\n", 
                           total_received, total_sent);
                InfraxSocketClass.free(socket);
                return -1;
            }
        }
    } else {
        core->printf(core, "Failed to connect after %d retries\n", 3);
        InfraxSocketClass.free(socket);
        return -1;
    }
    
    InfraxSocketClass.free(socket);
    core->printf(core, "Network error recovery test completed\n");
    return 0;
}

// 测试大数据传输
static int test_net_large_data() {
    core->printf(core, "Testing large data transfer...\n");

    // 等待TCP服务器就绪
    InfraxError err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in large data test: %s\n", err.message);
        return -1;
    }
    
    while (!tcp_server_ready) {
        err = test_cond->cond_timedwait(test_cond, test_mutex, 1000);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait for condition in large data test: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            return -1;
        }
    }
    
    test_mutex->mutex_unlock(test_mutex);
    
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,  // 使用阻塞模式
        .reuse_addr = true,
        .send_timeout_ms = TEST_TRANSFER_TIMEOUT_MS,  // 使用定义的超时时间
        .recv_timeout_ms = TEST_TRANSFER_TIMEOUT_MS  // 使用定义的超时时间
    };

    // Create client socket
    InfraxSocket* client = InfraxSocketClass.new(&config);
    if (!client) {
        core->printf(core, "Failed to create client socket\n");
        return -1;
    }

    // Prepare test data
    char* send_buffer = malloc(TEST_LARGE_DATA_SIZE);
    char* recv_buffer = malloc(TEST_LARGE_DATA_SIZE);
    if (!send_buffer || !recv_buffer) {
        core->printf(core, "Failed to allocate buffers\n");
        free(send_buffer);
        free(recv_buffer);
        InfraxSocketClass.free(client);
        return -1;
    }

    // Fill test data with pattern
    for (size_t i = 0; i < TEST_LARGE_DATA_SIZE; i++) {
        send_buffer[i] = (char)(i % 256);
    }

    // Connect to server
    InfraxNetAddr server_addr;
    err = infrax_net_addr_from_string("127.0.0.1", 12345, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to create server address: %s\n", err.message);
        free(send_buffer);
        free(recv_buffer);
        InfraxSocketClass.free(client);
        return -1;
    }

    // Connect to server
    err = client->connect(client, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to connect: %s\n", err.message);
        free(send_buffer);
        free(recv_buffer);
        InfraxSocketClass.free(client);
        return -1;
    }

    core->printf(core, "Connected to server\n");

    // Wait for server to be ready
    core->sleep_ms(core, 100);

    // Send data in chunks
    size_t total_sent = 0;
    while (total_sent < TEST_LARGE_DATA_SIZE) {
        size_t remaining = TEST_LARGE_DATA_SIZE - total_sent;
        size_t to_send = remaining > TEST_CHUNK_SIZE ? TEST_CHUNK_SIZE : remaining;
        size_t sent;

        err = client->send(client, send_buffer + total_sent, to_send, &sent);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to send data: %s\n", err.message);
            free(send_buffer);
            free(recv_buffer);
            InfraxSocketClass.free(client);
            return -1;
        }

        total_sent += sent;
        core->printf(core, "Sent %zu/%zu bytes\n", total_sent, TEST_LARGE_DATA_SIZE);
        core->sleep_ms(core, 10);  // Short delay to avoid sending too fast
    }

    core->printf(core, "Successfully sent all %zu bytes\n", total_sent);

    // Receive data in chunks
    size_t total_received = 0;
    while (total_received < TEST_LARGE_DATA_SIZE) {
        size_t remaining = TEST_LARGE_DATA_SIZE - total_received;
        size_t to_receive = remaining > TEST_CHUNK_SIZE ? TEST_CHUNK_SIZE : remaining;
        size_t received;

        err = client->recv(client, recv_buffer + total_received, to_receive, &received);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Receive error: %s\n", err.message);
            free(send_buffer);
            free(recv_buffer);
            InfraxSocketClass.free(client);
            return -1;
        }

        if (received == 0) {
            core->printf(core, "Incomplete receive: got %zu of %zu bytes\n", 
                        total_received, TEST_LARGE_DATA_SIZE);
            free(send_buffer);
            free(recv_buffer);
            InfraxSocketClass.free(client);
            return -1;
        }

        total_received += received;
        core->printf(core, "Received %zu/%zu bytes\n", total_received, TEST_LARGE_DATA_SIZE);
        core->sleep_ms(core, 10);  // Short delay to avoid receiving too fast
    }

    // Verify data
    if (memcmp(send_buffer, recv_buffer, TEST_LARGE_DATA_SIZE) != 0) {
        core->printf(core, "Data verification failed\n");
        free(send_buffer);
        free(recv_buffer);
        InfraxSocketClass.free(client);
        return -1;
    }

    core->printf(core, "Data verification successful\n");

    // Clean up resources
    free(send_buffer);
    free(recv_buffer);
    InfraxSocketClass.free(client);

    core->printf(core, "Large data transfer test passed\n");
    return 0;
}

static int test_infrax_net() {
    InfraxError err = ensure_core_initialized();
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Core initialization failed: %s\n", err.message);
        return -1;
    }

    core->printf(core, "Starting InfraxNet tests...\n");

    // Create synchronization primitives
    if (!test_mutex) {
        test_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
        if (!test_mutex) {
            core->printf(core, "Failed to create test mutex\n");
            return -1;
        }
    }
    
    if (!test_cond) {
        test_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
        if (!test_cond) {
            core->printf(core, "Failed to create test condition\n");
            return -1;
        }
    }

    // Set server address
    err = infrax_net_addr_from_string("127.0.0.1", 12345, &tcp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to set TCP server address: %s\n", err.message);
        return -1;
    }

    // Create and start server thread
    tcp_server_ready = false;
    tcp_server_running = true;

    InfraxThreadConfig thread_config = {
        .name = "tcp_server",
        .func = tcp_server_thread,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };

    tcp_server_thread_handle = InfraxThreadClass.new(&thread_config);
    if (!tcp_server_thread_handle) {
        core->printf(core, "Failed to create TCP server thread\n");
        return -1;
    }

    err = tcp_server_thread_handle->start(tcp_server_thread_handle, tcp_server_thread, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to start TCP server thread: %s\n", err.message);
        InfraxThreadClass.free(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
        return -1;
    }

    // Wait for server to be ready
    err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex: %s\n", err.message);
        tcp_server_running = false;
        InfraxThreadClass.free(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
        return -1;
    }

    while (!tcp_server_ready) {
        err = test_cond->cond_timedwait(test_cond, test_mutex, 1000);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_SYNC_TIMEOUT) {
                core->printf(core, "Timeout waiting for TCP server to be ready\n");
                tcp_server_running = false;
                test_mutex->mutex_unlock(test_mutex);
                InfraxThreadClass.free(tcp_server_thread_handle);
                tcp_server_thread_handle = NULL;
                return -1;
            }
            core->printf(core, "Failed to wait for TCP server: %s\n", err.message);
            tcp_server_running = false;
            test_mutex->mutex_unlock(test_mutex);
            InfraxThreadClass.free(tcp_server_thread_handle);
            tcp_server_thread_handle = NULL;
            return -1;
        }
    }

    err = test_mutex->mutex_unlock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to unlock mutex: %s\n", err.message);
        tcp_server_running = false;
        InfraxThreadClass.free(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
        return -1;
    }

    // Wait for server to be fully ready
    core->sleep_ms(core, 100);

    // Run tests
    core->printf(core, "Testing socket configuration...\n");
    test_config();
    core->printf(core, "Socket configuration tests passed\n");

    if (test_tcp() != 0) {
        core->printf(core, "TCP tests failed\n");
        return -1;
    }

    if (test_udp() != 0) {
        core->printf(core, "UDP tests failed\n");
        return -1;
    }

    if (test_net_stress() != 0) {
        core->printf(core, "Network stress tests failed\n");
        return -1;
    }

    if (test_net_error_recovery() != 0) {
        core->printf(core, "Network error recovery tests failed\n");
        return -1;
    }
    
    if (test_net_large_data() != 0) {
        core->printf(core, "Large data transfer tests failed\n");
        return -1;
    }

    core->printf(core, "All infrax_net tests completed!\n");

    // Cleanup resources
    tcp_server_running = false;
    if (tcp_server_thread_handle) {
        void* thread_result;
        err = tcp_server_thread_handle->join(tcp_server_thread_handle, &thread_result);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to join TCP server thread: %s\n", err.message);
            // Continue cleanup despite error
        }
        InfraxThreadClass.free(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
    }

    if (test_mutex) {
        InfraxSyncClass.free(test_mutex);
        test_mutex = NULL;
    }

    if (test_cond) {
        InfraxSyncClass.free(test_cond);
        test_cond = NULL;
    }

    return 0;
}

int main(void) {
    InfraxError err = ensure_core_initialized();
    if (INFRAX_ERROR_IS_ERR(err)) {
        printf("Failed to initialize core: %s\n", err.message);
        return 1;
    }
    
    core->printf(core, "===================\n");
    core->printf(core, "Starting InfraxNet tests...\n");
    
    // Create test synchronization primitives
    test_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    test_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    
    if (!test_mutex || !test_cond) {
        core->printf(core, "Failed to create test synchronization primitives\n");
        return 1;
    }
    
    // Initialize server state
    tcp_server_ready = false;
    tcp_server_running = true;
    
    // Run basic configuration test
    test_config();
    
    // Start TCP server thread
    InfraxThreadConfig tcp_config = {
        .name = "tcp_server",
        .func = tcp_server_thread,
        .arg = NULL
    };
    InfraxThread* tcp_thread = InfraxThreadClass.new(&tcp_config);
    if (!tcp_thread) {
        core->printf(core, "Failed to create TCP server thread\n");
        goto cleanup;
    }
    
    err = tcp_thread->start(tcp_thread, tcp_server_thread, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to start TCP server thread: %s\n", err.message);
        goto cleanup;
    }
    
    // Wait for server to be ready
    err = test_mutex->mutex_lock(test_mutex);
    if (!INFRAX_ERROR_IS_ERR(err)) {
        while (!tcp_server_ready) {
            err = test_cond->cond_wait(test_cond, test_mutex);
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to wait for server ready: %s\n", err.message);
                test_mutex->mutex_unlock(test_mutex);
                goto cleanup;
            }
        }
        test_mutex->mutex_unlock(test_mutex);
    }
    
    // Run other tests
    int tcp_result = test_tcp();
    if (tcp_result != 0) {
        goto cleanup;
    }
    int udp_result = test_udp();
    if (udp_result != 0) {
        goto cleanup;
    }
    if (test_net_stress() != 0) {
        goto cleanup;
    }
    if (test_net_error_recovery() != 0) {
        goto cleanup;
    }
    if (test_net_large_data() != 0) {
        goto cleanup;
    }

cleanup:
    // Stop TCP server
    if (tcp_thread) {
        tcp_server_running = false;
        void* thread_result;
        err = tcp_thread->join(tcp_thread, &thread_result);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to join TCP server thread: %s\n", err.message);
            // Continue cleanup despite error
        }
        InfraxThreadClass.free(tcp_thread);
        tcp_thread = NULL;
    }

    // Free synchronization primitives
    if (test_mutex) {
        InfraxSyncClass.free(test_mutex);
        test_mutex = NULL;
    }

    if (test_cond) {
        InfraxSyncClass.free(test_cond);
        test_cond = NULL;
    }

    // Free core
    if (core) {
        InfraxCoreClass.free(core);
        core = NULL;
    }

    return 0;
}
