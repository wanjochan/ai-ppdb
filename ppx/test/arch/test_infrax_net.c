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

static InfraxCore* core = NULL;
static InfraxSync* test_mutex = NULL;
static InfraxSync* test_cond = NULL;
static bool tcp_server_ready = false;
static bool tcp_server_running = false;
static bool udp_server_ready = false;
static bool udp_server_running = false;
static InfraxNetAddr tcp_server_addr;
static InfraxNetAddr udp_server_addr;
static InfraxThread* tcp_server_thread_handle = NULL;  

// 添加线程安全的初始化标志
static bool core_initialized = false;
static InfraxSync* core_mutex = NULL;

// 线程安全的初始化函数
static InfraxError ensure_core_initialized() {
    if (core_initialized) return INFRAX_ERROR_OK_STRUCT;
    
    if (!core_mutex) {
        core_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
        if (!core_mutex) {
            return make_error(INFRAX_ERROR_SYNC_CREATE_FAILED, "Failed to create core mutex");
        }
    }
    
    core_mutex->mutex_lock(core_mutex);
    if (!core) {
        core = InfraxCoreClass.singleton();
        if (core) {
            core_initialized = true;
        }
    }
    core_mutex->mutex_unlock(core_mutex);
    
    return core_initialized ? INFRAX_ERROR_OK_STRUCT : 
           make_error(INFRAX_ERROR_CORE_INIT_FAILED, "Failed to initialize core");
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

static void* tcp_server_thread(void* arg) {
    (void)arg;
    InfraxSocket* server = NULL;
    InfraxSocket* client = NULL;
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 30000,  // 增加超时时间到30秒
        .recv_timeout_ms = 30000,
        .reuse_addr = true
    };
    
    server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "Failed to create TCP server socket\n");
        return NULL;
    }

    // 绑定服务器地址
    InfraxError err = server->bind(server, &tcp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to bind TCP server: %s\n", err.message);
        goto cleanup;
    }

    // 开始监听
    err = server->listen(server, 5);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to listen on TCP server: %s\n", err.message);
        goto cleanup;
    }

    // 通知主线程服务器已准备就绪
    test_mutex->mutex_lock(test_mutex);
    tcp_server_ready = true;
    test_cond->cond_broadcast(test_cond);  
    test_mutex->mutex_unlock(test_mutex);

    char* buffer = malloc(8192);  // 8KB缓冲区
    if (!buffer) {
        core->printf(core, "Failed to allocate server buffer\n");
        goto cleanup;
    }

    while (tcp_server_running) {
        // 清理之前的客户端连接
        if (client) {
            InfraxSocketClass.free(client);
            client = NULL;
        }

        // 接受新连接
        InfraxNetAddr client_addr;
        err = server->accept(server, &client, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                core->sleep_ms(core, 1);  
                continue;
            }
            core->printf(core, "Failed to accept client: %s\n", err.message);
            continue;
        }

        // 设置客户端socket配置
        client->config.is_nonblocking = false;
        client->config.send_timeout_ms = 30000;
        client->config.recv_timeout_ms = 30000;
        client->config.reuse_addr = false;

        // 处理客户端数据
        bool client_error = false;
        while (tcp_server_running && !client_error) {
            size_t received;
            err = client->recv(client, buffer, 8192, &received);
            
            if (INFRAX_ERROR_IS_ERR(err)) {
                if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                    core->sleep_ms(core, 1);  
                    continue;
                }
                core->printf(core, "Failed to receive data: %s\n", err.message);
                client_error = true;
                break;
            }

            if (received == 0) {
                break;  // 客户端关闭连接
            }

            size_t total_sent = 0;
            while (total_sent < received && !client_error) {
                size_t sent;
                err = client->send(client, buffer + total_sent, received - total_sent, &sent);
                
                if (INFRAX_ERROR_IS_ERR(err)) {
                    if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                        core->sleep_ms(core, 1);  
                        continue;
                    }
                    core->printf(core, "Failed to send data: %s\n", err.message);
                    client_error = true;
                    break;
                }
                
                total_sent += sent;
            }

            if (!client_error) {
                core->printf(core, "Server echoed %zu bytes\n", total_sent);
            }
        }

        // 关闭客户端连接
        if (client) {
            InfraxSocketClass.free(client);
            client = NULL;
        }
    }

    free(buffer);

cleanup:
    if (client) {
        InfraxSocketClass.free(client);
    }
    if (server) {
        InfraxSocketClass.free(server);
    }
    return NULL;
}

static void* udp_server_thread(void* arg) {
    (void)arg;
    InfraxSocket* server = NULL;
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 5000,
        .recv_timeout_ms = 5000,
        .reuse_addr = true
    };
    
    server = InfraxSocketClass.new(&config);
    if (!server) {
        core->printf(core, "Failed to create UDP server socket\n");
        return NULL;
    }

    // 绑定服务器地址
    udp_server_addr.port = 12346;  // 使用固定端口
    InfraxError err = server->bind(server, &udp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to bind UDP server: %s\n", err.message);
        goto cleanup;
    }

    // 通知主线程服务器已准备就绪
    test_mutex->mutex_lock(test_mutex);
    udp_server_ready = true;
    test_cond->cond_broadcast(test_cond);  
    test_mutex->mutex_unlock(test_mutex);

    char buffer[1024] = {0};  
    while (udp_server_running) {
        size_t received;
        InfraxNetAddr client_addr;
        err = server->recvfrom(server, buffer, sizeof(buffer), &received, &client_addr);
        
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                core->sleep_ms(core, 1);  
                continue;
            }
            core->printf(core, "Failed to receive UDP data: %s\n", err.message);
            break;
        }

        if (received > 0) {
            size_t total_sent = 0;
            while (total_sent < received && udp_server_running) {
                size_t sent;
                err = server->sendto(server, buffer + total_sent, received - total_sent, &sent, &client_addr);
                
                if (INFRAX_ERROR_IS_ERR(err)) {
                    if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                        core->sleep_ms(core, 1);  
                        continue;
                    }
                    core->printf(core, "Failed to send UDP data: %s\n", err.message);
                    break;
                }
                
                total_sent += sent;
            }
            core->printf(core, "UDP server echoed %zu bytes\n", total_sent);
        }
    }

cleanup:
    if (server) {
        InfraxSocketClass.free(server);
    }
    return NULL;
}

static int test_tcp() {
    InfraxError err = ensure_core_initialized();
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Core initialization failed: %s\n", err.message);
        return INFRAX_ERROR_INVALID_DATA;
    }

    core->printf(core, "Testing TCP functionality...\n");
    
    // 创建同步原语（如果还没创建）
    if (!test_mutex) {
        test_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
        if (!test_mutex) {
            core->printf(core, "Failed to create test mutex\n");
            return INFRAX_ERROR_INVALID_DATA;
        }
    }
    
    if (!test_cond) {
        test_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
        if (!test_cond) {
            core->printf(core, "Failed to create test condition\n");
            return INFRAX_ERROR_INVALID_DATA;
        }
    }

    // 设置服务器地址
    err = infrax_net_addr_from_string("127.0.0.1", 12345, &tcp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to set TCP server address: %s\n", err.message);
        return INFRAX_ERROR_INVALID_DATA;
    }

    // 创建并启动服务器线程
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
        return INFRAX_ERROR_INVALID_DATA;
    }

    err = tcp_server_thread_handle->start(tcp_server_thread_handle, tcp_server_thread, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to start TCP server thread: %s\n", err.message);
        InfraxThreadClass.free(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
        return INFRAX_ERROR_INVALID_DATA;
    }

    // 等待服务器就绪
    test_mutex->mutex_lock(test_mutex);
    while (!tcp_server_ready) {
        err = test_cond->cond_timedwait(test_cond, test_mutex, 1000);  
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_SYNC_TIMEOUT) {
                core->printf(core, "Timeout waiting for TCP server to be ready\n");
                tcp_server_running = false;
                test_mutex->mutex_unlock(test_mutex);
                InfraxThreadClass.free(tcp_server_thread_handle);
                tcp_server_thread_handle = NULL;
                return INFRAX_ERROR_NET_TIMEOUT;
            }
            core->printf(core, "Failed to wait for TCP server: %s\n", err.message);
            tcp_server_running = false;
            test_mutex->mutex_unlock(test_mutex);
            InfraxThreadClass.free(tcp_server_thread_handle);
            tcp_server_thread_handle = NULL;
            return INFRAX_ERROR_INVALID_DATA;
        }
    }
    test_mutex->mutex_unlock(test_mutex);

    // 创建客户端socket
    InfraxSocketConfig client_config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = 5000,
        .recv_timeout_ms = 5000,
        .reuse_addr = false
    };

    InfraxSocket* client = InfraxSocketClass.new(&client_config);
    if (!client) {
        core->printf(core, "Failed to create TCP client socket\n");
        tcp_server_running = false;
        InfraxThreadClass.free(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
        return INFRAX_ERROR_INVALID_DATA;
    }

    // 连接到服务器
    err = client->connect(client, &tcp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to connect to TCP server: %s\n", err.message);
        InfraxSocketClass.free(client);
        tcp_server_running = false;
        InfraxThreadClass.free(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
        return INFRAX_ERROR_INVALID_DATA;
    }

    // 测试数据传输
    const struct {
        const char* data;
        size_t size;
    } test_patterns[] = {
        {"Hello, World!\n", 14},
        {"This is a test message.\n", 23},
        {"Pattern 3 - Longer message for testing.\n", 37},
        {"Pattern 4 - Even longer message for comprehensive testing.\n", 54}
    };

    for (size_t i = 0; i < sizeof(test_patterns)/sizeof(test_patterns[0]); i++) {
        core->printf(core, "Testing pattern %zu...\n", i + 1);
        
        // 发送数据
        size_t total_sent = 0;
        while (total_sent < test_patterns[i].size) {
            size_t sent;
            err = client->send(client, test_patterns[i].data + total_sent, 
                             test_patterns[i].size - total_sent, &sent);
            
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to send pattern %zu: %s\n", i + 1, err.message);
                InfraxSocketClass.free(client);
                tcp_server_running = false;
                InfraxThreadClass.free(tcp_server_thread_handle);
                tcp_server_thread_handle = NULL;
                return INFRAX_ERROR_INVALID_DATA;
            }
            
            total_sent += sent;
        }

        // 接收回显
        char recv_buffer[1024] = {0};
        size_t total_received = 0;
        
        while (total_received < test_patterns[i].size) {
            size_t received;
            err = client->recv(client, recv_buffer + total_received, 
                             sizeof(recv_buffer) - total_received, &received);
            
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to receive echo for pattern %zu: %s\n", 
                           i + 1, err.message);
                InfraxSocketClass.free(client);
                tcp_server_running = false;
                InfraxThreadClass.free(tcp_server_thread_handle);
                tcp_server_thread_handle = NULL;
                return INFRAX_ERROR_INVALID_DATA;
            }
            
            total_received += received;
        }

        // 验证数据
        if (total_received != test_patterns[i].size || 
            memcmp(recv_buffer, test_patterns[i].data, test_patterns[i].size) != 0) {
            core->printf(core, "Data mismatch for pattern %zu\n", i + 1);
            InfraxSocketClass.free(client);
            tcp_server_running = false;
            InfraxThreadClass.free(tcp_server_thread_handle);
            tcp_server_thread_handle = NULL;
            return INFRAX_ERROR_INVALID_DATA;
        }

        core->printf(core, "Server echoed %zu bytes\n", total_received);
        core->printf(core, "Pattern %zu test passed\n", i + 1);
        core->sleep_ms(core, 100);  
    }

    InfraxSocketClass.free(client);
    core->printf(core, "All TCP tests passed\n");
    return INFRAX_ERROR_OK;
}

static int test_udp() {
    InfraxError err = ensure_core_initialized();
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Core initialization failed: %s\n", err.message);
        return INFRAX_ERROR_INVALID_DATA;
    }

    core->printf(core, "Testing UDP functionality...\n");
    
    // 创建同步原语（如果还没创建）
    if (!test_mutex) {
        test_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
        if (!test_mutex) {
            core->printf(core, "Failed to create test mutex\n");
            return INFRAX_ERROR_INVALID_DATA;
        }
    }
    
    if (!test_cond) {
        test_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
        if (!test_cond) {
            core->printf(core, "Failed to create test condition\n");
            return INFRAX_ERROR_INVALID_DATA;
        }
    }

    // 设置服务器地址
    err = infrax_net_addr_from_string("127.0.0.1", 12346, &udp_server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to set UDP server address: %s\n", err.message);
        return INFRAX_ERROR_INVALID_DATA;
    }

    // 创建并启动服务器线程
    udp_server_ready = false;
    udp_server_running = true;

    InfraxThreadConfig thread_config = {
        .name = "udp_server",
        .func = udp_server_thread,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };

    InfraxThread* server_thread = InfraxThreadClass.new(&thread_config);
    if (!server_thread) {
        core->printf(core, "Failed to create UDP server thread\n");
        return INFRAX_ERROR_INVALID_DATA;
    }

    err = server_thread->start(server_thread, udp_server_thread, NULL);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to start UDP server thread: %s\n", err.message);
        InfraxThreadClass.free(server_thread);
        return INFRAX_ERROR_INVALID_DATA;
    }

    // 等待服务器就绪
    test_mutex->mutex_lock(test_mutex);
    while (!udp_server_ready) {
        err = test_cond->cond_timedwait(test_cond, test_mutex, 1000);  
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_SYNC_TIMEOUT) {
                core->printf(core, "Timeout waiting for UDP server to be ready\n");
                udp_server_running = false;
                test_mutex->mutex_unlock(test_mutex);
                InfraxThreadClass.free(server_thread);
                return INFRAX_ERROR_NET_TIMEOUT;
            }
            core->printf(core, "Failed to wait for UDP server: %s\n", err.message);
            udp_server_running = false;
            test_mutex->mutex_unlock(test_mutex);
            InfraxThreadClass.free(server_thread);
            return INFRAX_ERROR_INVALID_DATA;
        }
    }
    test_mutex->mutex_unlock(test_mutex);

    // 创建客户端socket
    InfraxSocketConfig client_config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = 5000,
        .recv_timeout_ms = 5000,
        .reuse_addr = false
    };

    InfraxSocket* client = InfraxSocketClass.new(&client_config);
    if (!client) {
        core->printf(core, "Failed to create UDP client socket\n");
        udp_server_running = false;
        InfraxThreadClass.free(server_thread);
        return INFRAX_ERROR_INVALID_DATA;
    }

    // 测试数据传输
    const struct {
        const char* data;
        size_t size;
    } test_patterns[] = {
        {"Hello, UDP World!\n", 16},
        {"This is a UDP test message.\n", 26},
        {"UDP Pattern 3 - Longer message for testing.\n", 41},
        {"UDP Pattern 4 - Even longer message for comprehensive testing.\n", 58}
    };

    for (size_t i = 0; i < sizeof(test_patterns)/sizeof(test_patterns[0]); i++) {
        core->printf(core, "Testing UDP pattern %zu...\n", i + 1);
        
        // 发送数据
        size_t total_sent = 0;
        while (total_sent < test_patterns[i].size) {
            size_t sent;
            err = client->sendto(client, test_patterns[i].data + total_sent, 
                               test_patterns[i].size - total_sent, &sent, &udp_server_addr);
            
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to send UDP pattern %zu: %s\n", i + 1, err.message);
                goto cleanup;
            }
            
            total_sent += sent;
        }

        // 接收回显
        char recv_buffer[1024] = {0};
        size_t total_received = 0;
        
        while (total_received < test_patterns[i].size) {
            size_t received;
            InfraxNetAddr server_addr;
            err = client->recvfrom(client, recv_buffer + total_received, 
                                 sizeof(recv_buffer) - total_received, &received, &server_addr);
            
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to receive UDP echo for pattern %zu: %s\n", 
                           i + 1, err.message);
                goto cleanup;
            }
            
            total_received += received;
        }

        // 验证数据
        if (total_received != test_patterns[i].size || 
            memcmp(recv_buffer, test_patterns[i].data, test_patterns[i].size) != 0) {
            core->printf(core, "UDP data mismatch for pattern %zu\n", i + 1);
            goto cleanup;
        }

        core->printf(core, "UDP pattern %zu test passed\n", i + 1);
        core->sleep_ms(core, 100);  
    }

    core->printf(core, "All UDP tests passed\n");

cleanup:
    // 清理资源
    InfraxSocketClass.free(client);
    udp_server_running = false;
    void* thread_result;
    server_thread->join(server_thread, &thread_result);
    InfraxThreadClass.free(server_thread);

    return INFRAX_ERROR_OK;
}

// 错误恢复测试
static void test_net_error_recovery() {
    core->printf(core, "Testing network error recovery...\n");
    
    // 等待TCP服务器就绪
    InfraxError err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in error recovery test: %s\n", err.message);
        return;
    }
    
    while (!tcp_server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait for condition in error recovery test: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            return;
        }
    }
    
    test_mutex->mutex_unlock(test_mutex);
    
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,  // 使用阻塞模式
        .send_timeout_ms = 5000,  // 增加到5秒超时
        .recv_timeout_ms = 5000,  // 增加到5秒超时
        .reuse_addr = true
    };
    
    InfraxSocket* socket = InfraxSocketClass.new(&config);
    if (!socket) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, 
            "socket != NULL", "Failed to create error recovery test socket");
        return;
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
    } else {
        core->printf(core, "Expected error connecting to invalid address: %s\n", err.message);
    }
    
    // 测试重连机制
    int retry_count = 5;  // 增加重试次数
    bool connected = false;
    core->printf(core, "Testing reconnection mechanism...\n");
    
    while (retry_count-- > 0 && !connected) {
        core->printf(core, "Connection attempt %d...\n", 5 - retry_count);
        err = socket->connect(socket, &tcp_server_addr);
        if (!INFRAX_ERROR_IS_ERR(err)) {
            connected = true;
            core->printf(core, "Successfully connected on attempt %d\n", 5 - retry_count);
            break;
        }
        core->printf(core, "Retrying connection: %s\n", err.message);
        core->sleep_ms(core, 500);  // 增加重试间隔
    }
    
    if (connected) {
        // 发送一些数据
        const char* test_data = "Test error recovery";
        size_t data_len = core->strlen(core, test_data);
        size_t total_sent = 0;
        
        core->printf(core, "Sending test data...\n");
        while (total_sent < data_len) {
            size_t sent;
            err = socket->send(socket, test_data + total_sent, data_len - total_sent, &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                    core->sleep_ms(core, 10);
                    continue;
                }
                core->printf(core, "Failed to send data: %s\n", err.message);
                break;
            }
            total_sent += sent;
            core->printf(core, "Sent %zu/%zu bytes\n", total_sent, data_len);
        }
        
        if (total_sent == data_len) {
            core->printf(core, "Successfully sent %zu bytes\n", total_sent);
            
            // 等待回显
            char recv_buf[64] = {0};
            size_t total_received = 0;
            int recv_retries = 100;  // 增加重试次数
            
            core->printf(core, "Waiting for echo response...\n");
            while (total_received < total_sent && recv_retries > 0) {
                size_t received;
                err = socket->recv(socket, recv_buf + total_received, 
                                 sizeof(recv_buf) - total_received, &received);
                if (INFRAX_ERROR_IS_ERR(err)) {
                    if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                        core->sleep_ms(core, 50);  // 增加等待时间
                        recv_retries--;
                        if (recv_retries % 10 == 0) {  // 每10次重试打印一次状态
                            core->printf(core, "Still waiting for data, %d retries left...\n", recv_retries);
                        }
                        continue;
                    }
                    core->printf(core, "Failed to receive data: %s\n", err.message);
                    break;
                }
                if (received == 0) {
                    core->printf(core, "Connection closed by peer\n");
                    break;
                }
                total_received += received;
                core->printf(core, "Received %zu/%zu bytes\n", total_received, total_sent);
            }
            
            if (recv_retries == 0) {
                core->printf(core, "Receive operation timed out after %d retries\n", 100);
            } else if (total_received == total_sent) {
                core->printf(core, "Successfully received %zu bytes\n", total_received);
                // 验证数据
                if (core->strncmp(core, recv_buf, test_data, total_received) != 0) {
                    core->printf(core, "Data verification failed!\n");
                    core->printf(core, "Expected: %.*s\n", (int)total_sent, test_data);
                    core->printf(core, "Received: %.*s\n", (int)total_received, recv_buf);
                    core->assert_failed(core, __FILE__, __LINE__, __func__, 
                        "Data verification", "Received data does not match sent data");
                } else {
                    core->printf(core, "Data verification successful\n");
                }
            } else {
                core->printf(core, "Incomplete receive: got %zu of %zu bytes\n", 
                           total_received, total_sent);
            }
        }
    } else {
        core->printf(core, "Failed to connect after %d retries\n", 5);
    }
    
    InfraxSocketClass.free(socket);
    core->printf(core, "Network error recovery test completed\n");
}

// 压力测试
static void test_net_stress() {
    core->printf(core, "Testing network stress...\n");
    
    // 等待TCP服务器就绪
    InfraxError err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in stress test: %s\n", err.message);
        return;
    }
    
    while (!tcp_server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait for condition in stress test: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            return;
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
}

// 修改大数据传输测试函数
static void test_net_large_data() {
    core->printf(core, "Testing large data transfer...\n");
    
    // 等待TCP服务器就绪
    InfraxError err = test_mutex->mutex_lock(test_mutex);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to lock mutex in large data test: %s\n", err.message);
        return;
    }
    
    while (!tcp_server_ready) {
        err = test_cond->cond_wait(test_cond, test_mutex);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "Failed to wait for condition in large data test: %s\n", err.message);
            test_mutex->mutex_unlock(test_mutex);
            return;
        }
    }
    
    test_mutex->mutex_unlock(test_mutex);
    
    #define LARGE_DATA_SIZE (16 * 1024)  // 16KB total data
    #define CHUNK_SIZE (2048)            // 2KB per chunk
    #define POLL_TIMEOUT_MS 1000         // 1 second poll timeout
    #define TRANSFER_TIMEOUT_MS 10000    // 10 second total timeout

    InfraxSocketConfig server_config = {
        .is_udp = false,
        .is_nonblocking = true,  // Enable non-blocking mode
        .reuse_addr = true,
        .send_timeout_ms = 5000,
        .recv_timeout_ms = 5000
    };

    InfraxSocketConfig client_config = {
        .is_udp = false,
        .is_nonblocking = true,  // Enable non-blocking mode
        .reuse_addr = true,
        .send_timeout_ms = 5000,
        .recv_timeout_ms = 5000
    };

    // Create server socket
    InfraxSocket* server = InfraxSocketClass.new(&server_config);
    ASSERT(server != NULL);

    // Create client socket
    InfraxSocket* client = InfraxSocketClass.new(&client_config);
    ASSERT(client != NULL);

    // Prepare test data
    char* test_data = malloc(LARGE_DATA_SIZE);
    char* recv_buffer = malloc(LARGE_DATA_SIZE);
    ASSERT(test_data != NULL && recv_buffer != NULL);

    // Fill test data with pattern
    for (size_t i = 0; i < LARGE_DATA_SIZE; i++) {
        test_data[i] = (char)(i & 0xFF);
    }

    // Bind server socket
    InfraxNetAddr server_addr = {
        .ip = "127.0.0.1",
        .port = 12345
    };
    ASSERT(INFRAX_ERROR_IS_OK(server->bind(server, &server_addr)));
    ASSERT(INFRAX_ERROR_IS_OK(server->listen(server, 1)));

    // Connect client socket
    ASSERT(INFRAX_ERROR_IS_OK(client->connect(client, &server_addr)));

    // Accept client connection
    InfraxSocket* server_client = NULL;
    InfraxNetAddr client_addr;
    
    // Setup poll for accept
    struct pollfd accept_poll = {
        .fd = server->native_handle,
        .events = POLLIN,
        .revents = 0
    };

    // Wait for client connection
    int poll_result = poll(&accept_poll, 1, POLL_TIMEOUT_MS);
    ASSERT(poll_result > 0);
    ASSERT(INFRAX_ERROR_IS_OK(server->accept(server, &server_client, &client_addr)));

    // Setup poll structures for data transfer
    struct pollfd poll_fds[2] = {
        {
            .fd = client->native_handle,
            .events = POLLOUT,  // Client starts by sending
            .revents = 0
        },
        {
            .fd = server_client->native_handle,
            .events = POLLIN,   // Server starts by receiving
            .revents = 0
        }
    };

    // Transfer state
    size_t total_sent = 0;
    size_t total_received = 0;
    int64_t start_time = core->time_monotonic_ms(core);

    // Data transfer loop
    while (total_sent < LARGE_DATA_SIZE || total_received < LARGE_DATA_SIZE) {
        int64_t current_time = core->time_monotonic_ms(core);
        if (current_time - start_time > TRANSFER_TIMEOUT_MS) {
            core->printf(core, "Transfer timeout after %d ms\n", TRANSFER_TIMEOUT_MS);
            break;
        }

        int poll_result = poll(poll_fds, 2, 10);  // 10ms poll timeout
        if (poll_result < 0) {
            core->printf(core, "Poll error: %s\n", strerror(errno));
            break;
        }

        // Handle client send
        if (total_sent < LARGE_DATA_SIZE && (poll_fds[0].revents & POLLOUT)) {
            size_t remaining = LARGE_DATA_SIZE - total_sent;
            size_t chunk = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
            size_t sent = 0;
            
            InfraxError err = client->send(client, test_data + total_sent, chunk, &sent);
            if (INFRAX_ERROR_IS_OK(err)) {
                total_sent += sent;
                if (total_sent % (CHUNK_SIZE * 4) == 0) {  // 每4个块打印一次
                    core->printf(core, "Client sent %zu bytes, total %zu/%zu\n", sent, total_sent, LARGE_DATA_SIZE);
                }
            } else if (err.code != INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                core->printf(core, "Send error: %s\n", err.message);
                break;
            }
        }

        // Handle server receive
        if (total_received < LARGE_DATA_SIZE && (poll_fds[1].revents & POLLIN)) {
            size_t remaining = LARGE_DATA_SIZE - total_received;
            size_t chunk = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
            size_t received = 0;
            
            InfraxError err = server_client->recv(server_client, recv_buffer + total_received, chunk, &received);
            if (INFRAX_ERROR_IS_OK(err)) {
                total_received += received;
                if (total_received % (CHUNK_SIZE * 4) == 0) {  // 每4个块打印一次
                    core->printf(core, "Server received %zu bytes, total %zu/%zu\n", received, total_received, LARGE_DATA_SIZE);
                }
            } else if (err.code != INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                core->printf(core, "Receive error: %s\n", err.message);
                break;
            }
        }

        // Reset events
        poll_fds[0].revents = 0;
        poll_fds[1].revents = 0;
    }

    // Verify received data
    ASSERT(total_sent == LARGE_DATA_SIZE);
    ASSERT(total_received == LARGE_DATA_SIZE);
    ASSERT(memcmp(test_data, recv_buffer, LARGE_DATA_SIZE) == 0);

    // Cleanup
    free(test_data);
    free(recv_buffer);
    InfraxSocketClass.free(server_client);
    InfraxSocketClass.free(client);
    InfraxSocketClass.free(server);
}

int main(void) {
    InfraxError err = ensure_core_initialized();
    if (INFRAX_ERROR_IS_ERR(err)) {
        printf("Failed to initialize core: %s\n", err.message);
        return 1;
    }
    
    core->printf(core, "===================\n");
    core->printf(core, "Starting InfraxNet tests...\n");
    
    // 创建测试同步原语
    test_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    test_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    
    if (!test_mutex || !test_cond) {
        core->printf(core, "Failed to create test synchronization primitives\n");
        return 1;
    }
    
    // 初始化服务器状态
    tcp_server_ready = false;
    tcp_server_running = true;
    udp_server_ready = false;
    udp_server_running = true;
    
    // 运行基础配置测试
    test_config();
    
    // 启动TCP服务器线程
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
    
    // 等待服务器就绪
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
    
    // 运行其他测试
    int tcp_result = test_tcp();
    if (tcp_result != INFRAX_ERROR_OK) {
        goto cleanup;
    }
    int udp_result = test_udp();
    if (udp_result != INFRAX_ERROR_OK) {
        goto cleanup;
    }
    test_net_stress();
    test_net_error_recovery();
    test_net_large_data();
    
    // 停止TCP服务器
    if (tcp_server_thread_handle) {
        tcp_server_running = false;
        void* thread_result;
        tcp_server_thread_handle->join(tcp_server_thread_handle, &thread_result);
        InfraxThreadClass.free(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
    }

cleanup:
    // 清理资源
    if (tcp_thread) {
        InfraxThreadClass.free(tcp_thread);
    }
    if (test_mutex) {
        InfraxSyncClass.free(test_mutex);
        test_mutex = NULL;
    }
    if (test_cond) {
        InfraxSyncClass.free(test_cond);
        test_cond = NULL;
    }
    if (core_mutex) {
        InfraxSyncClass.free(core_mutex);
        core_mutex = NULL;
    }

    core->printf(core, "All infrax_net tests completed!\n");
    core->printf(core, "===================\n");
    return 0;
}
