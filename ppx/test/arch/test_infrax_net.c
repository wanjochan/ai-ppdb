#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
/*
测试同步为主
*/
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>

// Test parameters
#define TEST_PORT_BASE 22345
#define TEST_TIMEOUT_SEC 5  // 增加到5秒
#define TEST_BUFFER_SIZE (128 * 1024)  // 增加到128KB
#define UDP_MAX_PACKET_SIZE 8192      // UDP包最大8KB
#define TEST_RETRY_COUNT 3
#define TEST_RETRY_DELAY_MS 100

// Flow control parameters
#define FLOW_CONTROL_CHUNK_SIZE (64 * 1024)  // 增加到64KB
#define FLOW_CONTROL_DELAY_MS 5  // 增加到5ms
#define PROGRESS_UPDATE_INTERVAL (256 * 1024)  // 256KB更新一次进度

// Socket buffer sizes
#define SOCKET_RCVBUF_SIZE (1 * 1024 * 1024)  // 增加到1MB
#define SOCKET_SNDBUF_SIZE (1 * 1024 * 1024)  // 增加到1MB

// Test logging
typedef enum {
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG
} LogLevel;

static void test_log(LogLevel level, const char* file, int line, const char* func, const char* fmt, ...) {
    static const char* level_str[] = {"ERROR", "WARN", "INFO", "DEBUG"};
    va_list args;
    char message[256];
    
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    printf("[%s] %s:%d %s: %s\n", level_str[level], file, line, func, message);
}

#define TEST_LOG_ERROR(...) test_log(LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TEST_LOG_WARN(...) test_log(LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TEST_LOG_INFO(...) test_log(LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define TEST_LOG_DEBUG(...) test_log(LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

// Server context
typedef struct {
    InfraxSocket* socket;
    InfraxThread* thread;
    InfraxSync* ready_mutex;
    InfraxSync* ready_cond;
    volatile bool is_ready;
    volatile bool is_running;
    struct {
        uint32_t total_bytes;
        uint32_t total_packets;
        uint32_t errors;
    } stats;
    uint16_t port;
    bool is_udp;
} ServerContext;

// Test case structure
typedef struct {
    const char* name;
    bool (*setup)(void* arg);
    bool (*run)(void* arg);
    void (*cleanup)(void* arg);
    int timeout_ms;
    void* arg;
} TestCase;

// Test suite structure
typedef struct {
    const char* name;
    TestCase* cases;
    size_t case_count;
    bool (*before_all)(void);
    void (*after_all)(void);
} TestSuite;

// Test result structure
typedef struct {
    const char* suite_name;
    const char* case_name;
    bool passed;
    char message[256];
    uint64_t duration_ms;
} TestResult;

// Global variables
static InfraxCore* core = NULL;
static TestResult* results = NULL;
static size_t result_count = 0;

// Server context management
static ServerContext* create_server_context(bool is_udp, uint16_t port) {
    ServerContext* ctx = calloc(1, sizeof(ServerContext));
    if (!ctx) {
        TEST_LOG_ERROR("Failed to allocate server context");
        return NULL;
    }
    
    ctx->ready_mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    ctx->ready_cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    
    if (!ctx->ready_mutex || !ctx->ready_cond) {
        TEST_LOG_ERROR("Failed to create synchronization primitives");
        goto error;
    }
    
    InfraxSocketConfig config = {
        .is_udp = is_udp,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_SEC * 1000,
        .recv_timeout_ms = TEST_TIMEOUT_SEC * 1000,
        .reuse_addr = true
    };
    
    ctx->socket = InfraxSocketClass.new(&config);
    if (!ctx->socket) {
        TEST_LOG_ERROR("Failed to create socket");
        goto error;
    }

    // Set socket buffer sizes
    InfraxError err = ctx->socket->set_option(ctx->socket, INFRAX_SOL_SOCKET, INFRAX_SO_RCVBUF, 
                                            &(int){SOCKET_RCVBUF_SIZE}, sizeof(int));
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_WARN("Failed to set receive buffer size: %s", err.message);
    }

    err = ctx->socket->set_option(ctx->socket, INFRAX_SOL_SOCKET, INFRAX_SO_SNDBUF,
                                &(int){SOCKET_SNDBUF_SIZE}, sizeof(int));
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_WARN("Failed to set send buffer size: %s", err.message);
    }
    
    ctx->is_udp = is_udp;
    ctx->port = port;
    return ctx;

error:
    if (ctx->ready_mutex) InfraxSyncClass.free(ctx->ready_mutex);
    if (ctx->ready_cond) InfraxSyncClass.free(ctx->ready_cond);
    if (ctx->socket) InfraxSocketClass.free(ctx->socket);
    free(ctx);
    return NULL;
}

static void destroy_server_context(ServerContext* ctx) {
    if (!ctx) return;
    
    ctx->is_running = false;
    
    // First close the socket to unblock any pending operations
    if (ctx->socket) {
        ctx->socket->shutdown(ctx->socket, INFRAX_SHUT_RDWR);
        InfraxSocketClass.free(ctx->socket);
        ctx->socket = NULL;
    }
    
    // Then wait for the thread to exit
    if (ctx->thread) {
        void* thread_result;
        InfraxThreadClass.join(ctx->thread, &thread_result);
        InfraxThreadClass.free(ctx->thread);
    }
    
    if (ctx->ready_mutex) InfraxSyncClass.free(ctx->ready_mutex);
    if (ctx->ready_cond) InfraxSyncClass.free(ctx->ready_cond);
    
    free(ctx);
}

static int send_all(int sockfd, const void *buf, size_t len) {
    const char *ptr = buf;
    size_t remaining = len;
    const int max_retries = 3;
    const int retry_delay_ms = 50;
    size_t total_sent = 0;
    
    while (remaining > 0) {
        int retry_count = 0;
        while (retry_count < max_retries) {
            size_t to_send = remaining > FLOW_CONTROL_CHUNK_SIZE ? FLOW_CONTROL_CHUNK_SIZE : remaining;
            
            ssize_t n = send(sockfd, ptr, to_send, 0);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    fd_set wfds;
                    struct timeval tv;
                    FD_ZERO(&wfds);
                    FD_SET(sockfd, &wfds);
                    tv.tv_sec = 1;
                    tv.tv_usec = 0;
                    
                    int ret = select(sockfd + 1, NULL, &wfds, NULL, &tv);
                    if (ret < 0) {
                        if (errno == EINTR) {
                            retry_count++;
                            continue;
                        }
                        TEST_LOG_ERROR("Select error: %s", strerror(errno));
                        return -1;
                    }
                    if (ret == 0) {
                        retry_count++;
                        TEST_LOG_WARN("Select timeout, retry %d/%d", retry_count, max_retries);
                        core->sleep_ms(core, retry_delay_ms);
                        continue;
                    }
                    continue;
                }
                TEST_LOG_ERROR("Send error: %s", strerror(errno));
                return -1;
            }
            
            ptr += n;
            remaining -= n;
            total_sent += n;
            
            // 流控：每发送一个chunk后暂停
            if (total_sent % FLOW_CONTROL_CHUNK_SIZE == 0) {
                core->sleep_ms(core, FLOW_CONTROL_DELAY_MS);
                TEST_LOG_DEBUG("Flow control pause after sending %zu bytes", total_sent);
            }
            
            // 更新进度
            if (total_sent % PROGRESS_UPDATE_INTERVAL == 0) {
                TEST_LOG_INFO("Total sent: %zu bytes", total_sent);
            }
            
            break;
        }
        
        if (retry_count >= max_retries) {
            TEST_LOG_ERROR("Max retries reached");
            errno = ETIMEDOUT;
            return -1;
        }
    }
    
    TEST_LOG_DEBUG("Successfully sent all %zu bytes", total_sent);
    return 0;
}

// TCP server implementation
static void* tcp_server_thread(void* arg) {
    ServerContext* ctx = (ServerContext*)arg;
    InfraxError err;
    InfraxSocket* client = NULL;
    
    // 使用双缓冲
    char* recv_buffer = malloc(SOCKET_RCVBUF_SIZE);  // 使用更大的接收缓冲区
    char* send_buffer = malloc(SOCKET_SNDBUF_SIZE);  // 使用更大的发送缓冲区
    if (!recv_buffer || !send_buffer) {
        TEST_LOG_ERROR("Failed to allocate buffers");
        free(recv_buffer);
        free(send_buffer);
        return NULL;
    }
    
    // Bind and listen
    InfraxNetAddr addr;
    err = infrax_net_addr_from_string("127.0.0.1", ctx->port, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to create address: %s", err.message);
        free(recv_buffer);
        free(send_buffer);
        return NULL;
    }
    
    err = ctx->socket->bind(ctx->socket, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to bind: %s", err.message);
        free(recv_buffer);
        free(send_buffer);
        return NULL;
    }
    
    err = ctx->socket->listen(ctx->socket, 5);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to listen: %s", err.message);
        free(recv_buffer);
        free(send_buffer);
        return NULL;
    }
    
    // Signal ready
    ctx->ready_mutex->klass->mutex_lock(ctx->ready_mutex);
    ctx->is_ready = true;
    ctx->ready_cond->klass->cond_signal(ctx->ready_cond);
    ctx->ready_mutex->klass->mutex_unlock(ctx->ready_mutex);
    
    TEST_LOG_INFO("TCP server ready on port %d", ctx->port);
    
    // Main loop
    while (ctx->is_running) {
        InfraxNetAddr client_addr;
        err = ctx->socket->accept(ctx->socket, &client, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (!ctx->is_running) break;
            TEST_LOG_ERROR("Accept failed: %s", err.message);
            continue;
        }
        
        // Set socket buffer sizes for client
        err = client->set_option(client, INFRAX_SOL_SOCKET, INFRAX_SO_RCVBUF,
                               &(int){SOCKET_RCVBUF_SIZE}, sizeof(int));
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_WARN("Failed to set client receive buffer size: %s", err.message);
        }
        
        err = client->set_option(client, INFRAX_SOL_SOCKET, INFRAX_SO_SNDBUF,
                               &(int){SOCKET_SNDBUF_SIZE}, sizeof(int));
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_WARN("Failed to set client send buffer size: %s", err.message);
        }
        
        TEST_LOG_INFO("Client connected from %s:%d", client_addr.ip, client_addr.port);
        
        // Echo loop
        size_t total_received = 0;
        size_t last_progress = 0;
        size_t buffer_pos = 0;
        
        while (ctx->is_running) {
            // 确保缓冲区有足够空间
            size_t available = SOCKET_RCVBUF_SIZE - buffer_pos;
            if (available == 0) {
                TEST_LOG_ERROR("Receive buffer full");
                goto client_cleanup;
            }
            
            ssize_t bytes_received = recv(client->native_handle, 
                              recv_buffer + buffer_pos,
                              available,  // 尽可能多地接收数据
                              0);
            
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    TEST_LOG_INFO("Client closed connection normally");
                } else {
                    TEST_LOG_ERROR("Receive error: Failed to receive data");
                }
                goto client_cleanup;
            }
            
            buffer_pos += bytes_received;
            total_received += bytes_received;
            
            // 立即回复数据
            if (send_all(client->native_handle, recv_buffer, buffer_pos) < 0) {
                TEST_LOG_ERROR("Failed to echo data back to client");
                goto client_cleanup;
            }
            TEST_LOG_INFO("Server received and echoed %zu bytes", buffer_pos);
            buffer_pos = 0;
        }
        
client_cleanup:
        if (client) {
            InfraxSocketClass.free(client);
            client = NULL;
        }
    }
    
    free(recv_buffer);
    free(send_buffer);
    return NULL;
}

// UDP server implementation
static void* udp_server_thread(void* arg) {
    ServerContext* ctx = (ServerContext*)arg;
    InfraxError err;
    char buffer[TEST_BUFFER_SIZE];
    
    // Bind
    InfraxNetAddr addr;
    err = infrax_net_addr_from_string("127.0.0.1", ctx->port, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to create address: %s", err.message);
        return NULL;
    }
    
    err = ctx->socket->bind(ctx->socket, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to bind: %s", err.message);
        return NULL;
    }
    
    // Signal ready
    ctx->ready_mutex->klass->mutex_lock(ctx->ready_mutex);
    ctx->is_ready = true;
    ctx->ready_cond->klass->cond_signal(ctx->ready_cond);
    ctx->ready_mutex->klass->mutex_unlock(ctx->ready_mutex);
    
    TEST_LOG_INFO("UDP server ready on port %d", ctx->port);
    
    // Main loop
    while (ctx->is_running) {
        InfraxNetAddr client_addr;
        size_t received;
        err = ctx->socket->recvfrom(ctx->socket, buffer, sizeof(buffer), &received, &client_addr);
        
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (!ctx->is_running) break;  // Exit if server is shutting down
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                continue;
            }
            if (err.code == INFRAX_ERROR_NET_RECV_FAILED_CODE && strstr(err.message, "Bad file descriptor")) {
                // Socket has been closed
                break;
            }
            TEST_LOG_ERROR("Receive error: %s", err.message);
            ctx->stats.errors++;
            continue;
        }
        
        if (received == 0) continue;
        
        TEST_LOG_DEBUG("Received %zu bytes from %s:%d", 
                      received, client_addr.ip, client_addr.port);
        
        size_t sent;
        err = ctx->socket->sendto(ctx->socket, buffer, received, &sent, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_ERROR("Send error: %s", err.message);
            ctx->stats.errors++;
            continue;
        }
        
        ctx->stats.total_bytes += sent;
        ctx->stats.total_packets++;
    }
    
    return NULL;
}

// Basic test implementations
static bool test_tcp_basic(void* arg) {
    ServerContext* server = (ServerContext*)arg;
    InfraxError err;
    InfraxSocket* client = NULL;
    const char* test_data = "Hello, TCP!";
    char buffer[TEST_BUFFER_SIZE];
    bool success = false;
    
    // Create client
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_SEC * 1000,
        .recv_timeout_ms = TEST_TIMEOUT_SEC * 1000
    };
    
    client = InfraxSocketClass.new(&config);
    if (!client) {
        TEST_LOG_ERROR("Failed to create client socket");
        return false;
    }
    
    // Connect
    InfraxNetAddr server_addr;
    err = infrax_net_addr_from_string("127.0.0.1", server->port, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to create server address: %s", err.message);
        goto cleanup;
    }
    
    err = client->connect(client, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to connect: %s", err.message);
        goto cleanup;
    }
    
    // Send data
    size_t data_len = strlen(test_data);
    size_t sent;
    err = client->send(client, test_data, data_len, &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to send: %s", err.message);
        goto cleanup;
    }
    
    // Receive echo
    size_t received;
    err = client->recv(client, buffer, sizeof(buffer), &received);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to receive: %s", err.message);
        goto cleanup;
    }
    
    // Verify
    if (received != data_len || memcmp(test_data, buffer, data_len) != 0) {
        TEST_LOG_ERROR("Data verification failed");
        goto cleanup;
    }
    
    success = true;

cleanup:
    if (client) InfraxSocketClass.free(client);
    return success;
}

static bool test_udp_basic(void* arg) {
    ServerContext* server = (ServerContext*)arg;
    InfraxError err;
    InfraxSocket* client = NULL;
    const char* test_data = "Hello, UDP!";
    char buffer[TEST_BUFFER_SIZE];
    bool success = false;
    
    // Create client
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_SEC * 1000,
        .recv_timeout_ms = TEST_TIMEOUT_SEC * 1000
    };
    
    client = InfraxSocketClass.new(&config);
    if (!client) {
        TEST_LOG_ERROR("Failed to create client socket");
        return false;
    }
    
    // Send data
    InfraxNetAddr server_addr;
    err = infrax_net_addr_from_string("127.0.0.1", server->port, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to create server address: %s", err.message);
        goto cleanup;
    }
    
    size_t data_len = strlen(test_data);
    size_t sent;
    err = client->sendto(client, test_data, data_len, &sent, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to send: %s", err.message);
        goto cleanup;
    }
    
    // Receive echo
    InfraxNetAddr recv_addr;
    size_t received;
    err = client->recvfrom(client, buffer, sizeof(buffer), &received, &recv_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to receive: %s", err.message);
        goto cleanup;
    }
    
    // Verify
    if (received != data_len || memcmp(test_data, buffer, data_len) != 0) {
        TEST_LOG_ERROR("Data verification failed");
        goto cleanup;
    }
    
    success = true;

cleanup:
    if (client) InfraxSocketClass.free(client);
    return success;
}

// Server setup and cleanup
static bool setup_tcp_server(void* arg) {
    ServerContext* ctx = (ServerContext*)arg;
    InfraxError err;
    
    // Create thread
    InfraxThreadConfig config = {
        .name = "tcp_server",
        .func = tcp_server_thread,
        .arg = ctx
    };
    
    ctx->thread = InfraxThreadClass.new(&config);
    if (!ctx->thread) {
        TEST_LOG_ERROR("Failed to create server thread");
        return false;
    }
    
    ctx->is_running = true;
    err = InfraxThreadClass.start(ctx->thread, tcp_server_thread, ctx);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to start server thread: %s", err.message);
        return false;
    }
    
    // Wait for ready
    int retry = TEST_RETRY_COUNT;
    while (retry > 0 && !ctx->is_ready) {
        ctx->ready_mutex->klass->mutex_lock(ctx->ready_mutex);
        err = ctx->ready_cond->klass->cond_timedwait(ctx->ready_cond, 
                                            ctx->ready_mutex, 
                                            TEST_TIMEOUT_SEC * 1000);
        ctx->ready_mutex->klass->mutex_unlock(ctx->ready_mutex);
        
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_WARN("Waiting for server (%d retries left)", retry);
            core->sleep_ms(core, TEST_RETRY_DELAY_MS);
            retry--;
            continue;
        }
        break;
    }
    
    return ctx->is_ready;
}

static bool setup_udp_server(void* arg) {
    ServerContext* ctx = (ServerContext*)arg;
    InfraxError err;
    
    // Create thread
    InfraxThreadConfig config = {
        .name = "udp_server",
        .func = udp_server_thread,
        .arg = ctx
    };
    
    ctx->thread = InfraxThreadClass.new(&config);
    if (!ctx->thread) {
        TEST_LOG_ERROR("Failed to create server thread");
        return false;
    }
    
    ctx->is_running = true;
    err = InfraxThreadClass.start(ctx->thread, udp_server_thread, ctx);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to start server thread: %s", err.message);
        return false;
    }
    
    // Wait for ready
    int retry = TEST_RETRY_COUNT;
    while (retry > 0 && !ctx->is_ready) {
        ctx->ready_mutex->klass->mutex_lock(ctx->ready_mutex);
        err = ctx->ready_cond->klass->cond_timedwait(ctx->ready_cond, 
                                            ctx->ready_mutex, 
                                            TEST_TIMEOUT_SEC * 1000);
        ctx->ready_mutex->klass->mutex_unlock(ctx->ready_mutex);
        
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_WARN("Waiting for server (%d retries left)", retry);
            core->sleep_ms(core, TEST_RETRY_DELAY_MS);
            retry--;
            continue;
        }
        break;
    }
    
    return ctx->is_ready;
}

// Test suites
static TestCase basic_cases[] = {
    {
        .name = "tcp_basic",
        .setup = setup_tcp_server,
        .run = test_tcp_basic,
        .cleanup = NULL,
        .timeout_ms = TEST_TIMEOUT_SEC * 1000
    },
    {
        .name = "udp_basic",
        .setup = setup_udp_server,
        .run = test_udp_basic,
        .cleanup = NULL,
        .timeout_ms = TEST_TIMEOUT_SEC * 1000
    }
};

static TestSuite basic_suite = {
    .name = "basic",
    .cases = basic_cases,
    .case_count = sizeof(basic_cases) / sizeof(basic_cases[0])
};

// Error handling test implementations
static bool test_invalid_address(void* arg) {
    (void)arg;
    InfraxError err;
    InfraxSocket* socket = NULL;
    bool success = false;
    
    // Create socket
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_SEC * 1000,
        .recv_timeout_ms = TEST_TIMEOUT_SEC * 1000
    };
    
    socket = InfraxSocketClass.new(&config);
    if (!socket) {
        TEST_LOG_ERROR("Failed to create socket");
        return false;
    }
    
    // Test invalid IP address
    InfraxNetAddr invalid_addr;
    err = infrax_net_addr_from_string("256.256.256.256", TEST_PORT_BASE + 100, &invalid_addr);
    if (!INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Invalid IP address was accepted");
        goto cleanup;
    }
    TEST_LOG_INFO("Invalid IP address test passed");
    
    // Test empty IP address
    err = infrax_net_addr_from_string("", TEST_PORT_BASE + 100, &invalid_addr);
    if (!INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Empty IP address was accepted");
        goto cleanup;
    }
    TEST_LOG_INFO("Empty IP address test passed");
    
    // Test reserved ports (0 and 1-1023)
    err = infrax_net_addr_from_string("127.0.0.1", 0, &invalid_addr);
    if (!INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Port 0 was accepted");
        goto cleanup;
    }
    TEST_LOG_INFO("Port 0 test passed");
    
    err = infrax_net_addr_from_string("127.0.0.1", 22, &invalid_addr);
    if (!INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Reserved port 22 was accepted");
        goto cleanup;
    }
    TEST_LOG_INFO("Reserved port test passed");
    
    success = true;

cleanup:
    if (socket) InfraxSocketClass.free(socket);
    return success;
}

static bool test_port_in_use(void* arg) {
    (void)arg;
    InfraxError err;
    InfraxSocket* first = NULL;
    InfraxSocket* second = NULL;
    bool success = false;
    
    // Create first socket
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_SEC * 1000,
        .recv_timeout_ms = TEST_TIMEOUT_SEC * 1000,
        .reuse_addr = false
    };
    
    first = InfraxSocketClass.new(&config);
    if (!first) {
        TEST_LOG_ERROR("Failed to create first socket");
        return false;
    }
    
    // Bind first socket
    InfraxNetAddr addr;
    err = infrax_net_addr_from_string("127.0.0.1", TEST_PORT_BASE + 101, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to create address: %s", err.message);
        goto cleanup;
    }
    
    err = first->bind(first, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to bind first socket: %s", err.message);
        goto cleanup;
    }
    
    // Try to bind second socket to same port
    second = InfraxSocketClass.new(&config);
    if (!second) {
        TEST_LOG_ERROR("Failed to create second socket");
        goto cleanup;
    }
    
    err = second->bind(second, &addr);
    if (!INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Second bind succeeded when it should have failed");
        goto cleanup;
    }
    
    success = true;

cleanup:
    if (first) InfraxSocketClass.free(first);
    if (second) InfraxSocketClass.free(second);
    return success;
}

static bool test_connection_timeout(void* arg) {
    (void)arg;
    InfraxSocket* client_socket = NULL;
    InfraxError err;
    InfraxTime start_time, end_time;
    bool success = false;
    InfraxNetAddr addr;

    TEST_LOG_INFO("Starting connection timeout test");

    // 创建客户端socket，使用阻塞模式
    client_socket = InfraxSocketClass.new(&(InfraxSocketConfig){
        .is_udp = false,
        .is_nonblocking = false,  // 使用阻塞模式
        .send_timeout_ms = 500,
        .recv_timeout_ms = 500
    });
    if (!client_socket) {
        TEST_LOG_ERROR("Failed to create client socket");
        goto cleanup;
    }

    TEST_LOG_INFO("Creating client socket with timeout: 500 ms");

    // 使用一个不可达的地址（这个地址在RFC 5737中定义为测试用途）
    err = infrax_net_addr_from_string("192.0.2.1", 54321, &addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to create address: %s", err.message);
        goto cleanup;
    }

    // 记录开始时间
    start_time = core->time_monotonic_ms(core);
    TEST_LOG_INFO("Starting connection attempt at: %lu ms", start_time);

    // 尝试连接 - 这里应该超时
    err = client_socket->connect(client_socket, &addr);
    
    // 记录结束时间
    end_time = core->time_monotonic_ms(core);
    TEST_LOG_INFO("Connection attempt ended at: %lu ms", end_time);

    // 计算经过的时间
    InfraxTime elapsed = end_time - start_time;
    TEST_LOG_INFO("Connection attempt took %lu ms", elapsed);

    // 检查结果
    if (!INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Connection unexpectedly succeeded");
        goto cleanup;
    }

    if (err.code != INFRAX_ERROR_NET_TIMEOUT_CODE) {
        TEST_LOG_ERROR("Expected timeout error, got: %s", err.message);
        goto cleanup;
    }

    TEST_LOG_INFO("Connection failed as expected");

    // 检查经过的时间是否在合理范围内 (500ms +/- 100ms)
    if (elapsed < 400 || elapsed > 600) {
        TEST_LOG_ERROR("Connection timeout took %lu ms, expected ~500 ms", elapsed);
        goto cleanup;
    }

    TEST_LOG_INFO("Connection timeout test passed");
    success = true;

cleanup:
    if (client_socket) {
        TEST_LOG_INFO("Cleaning up client socket");
        InfraxSocketClass.free(client_socket);
    }
    return success;
}

// Boundary condition tests
static bool test_tcp_boundary(void* arg) {
    ServerContext* server = (ServerContext*)arg;
    InfraxError err;
    InfraxSocket* client = NULL;
    bool success = false;
    
    // Create client with longer timeouts
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_SEC * 1000 * 4,  // 进一步增加超时时间
        .recv_timeout_ms = TEST_TIMEOUT_SEC * 1000 * 4,
        .reuse_addr = true
    };
    
    client = InfraxSocketClass.new(&config);
    if (!client) {
        TEST_LOG_ERROR("Failed to create client socket");
        return false;
    }
    
    // Connect to server
    InfraxNetAddr server_addr;
    err = infrax_net_addr_from_string("127.0.0.1", server->port, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to create server address: %s", err.message);
        goto cleanup;
    }
    
    err = client->connect(client, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to connect: %s", err.message);
        goto cleanup;
    }
    
    // Test 1: Send zero bytes
    if (send_all(client->native_handle, "x", 0) < 0) {
        TEST_LOG_ERROR("Failed to send zero bytes: %s", strerror(errno));
        goto cleanup;
    }
    TEST_LOG_INFO("Zero bytes send test passed");
    
    // Test 2: Send maximum size packet
    char* large_buffer = malloc(TEST_BUFFER_SIZE);
    if (!large_buffer) {
        TEST_LOG_ERROR("Failed to allocate large buffer");
        goto cleanup;
    }
    memset(large_buffer, 'A', TEST_BUFFER_SIZE);
    
    if (send_all(client->native_handle, large_buffer, TEST_BUFFER_SIZE) < 0) {
        TEST_LOG_ERROR("Failed to send large buffer: %s", strerror(errno));
        free(large_buffer);
        goto cleanup;
    }
    free(large_buffer);
    TEST_LOG_INFO("Large buffer send test passed");
    
    // Test 3: Large file transfer (1MB)
    size_t large_file_size = 512 * 1024; // 512KB
    char* large_file_buffer = malloc(large_file_size);
    if (!large_file_buffer) {
        TEST_LOG_ERROR("Failed to allocate large file buffer");
        goto cleanup;
    }

    // Fill buffer with pattern data
    for (size_t i = 0; i < large_file_size; i++) {
        large_file_buffer[i] = 'A' + (i % 26);
    }

    // Send large file in chunks with progress tracking
    size_t total_sent = 0;
    size_t last_progress = 0;
    while (total_sent < large_file_size) {
        size_t chunk_size = large_file_size - total_sent;
        if (chunk_size > FLOW_CONTROL_CHUNK_SIZE) {
            chunk_size = FLOW_CONTROL_CHUNK_SIZE;
        }

        if (send_all(client->native_handle, large_file_buffer + total_sent, chunk_size) < 0) {
            TEST_LOG_ERROR("Failed to send large file chunk: %s", strerror(errno));
            free(large_file_buffer);
            goto cleanup;
        }
        
        total_sent += chunk_size;
        
        // 更新进度
        if (total_sent - last_progress >= PROGRESS_UPDATE_INTERVAL) {
            size_t progress_percent = (total_sent * 100) / large_file_size;
            TEST_LOG_INFO("Sent %zu bytes of large file (%zu%%)", total_sent, progress_percent);
            last_progress = total_sent;
        }
    }

    free(large_file_buffer);
    TEST_LOG_INFO("Large file transfer test passed");
    
    // Properly close the connection
    TEST_LOG_INFO("Closing client connection");
    if (shutdown(client->native_handle, SHUT_WR) < 0) {
        TEST_LOG_ERROR("Failed to shutdown client socket: %s", strerror(errno));
    }
    
    // Wait for server to process remaining data
    usleep(100000); // 100ms
    
    // Close the socket
    client->close(client);
    TEST_LOG_INFO("Client connection closed");
    
    success = true;

cleanup:
    if (client) InfraxSocketClass.free(client);
    return success;
}

static bool test_udp_boundary(void* arg) {
    ServerContext* server = (ServerContext*)arg;
    InfraxError err;
    InfraxSocket* client = NULL;
    bool success = false;
    
    // Create client
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_SEC * 1000,
        .recv_timeout_ms = TEST_TIMEOUT_SEC * 1000
    };
    
    client = InfraxSocketClass.new(&config);
    if (!client) {
        TEST_LOG_ERROR("Failed to create client socket");
        return false;
    }
    
    // Test 1: Send zero bytes
    InfraxNetAddr server_addr;
    err = infrax_net_addr_from_string("127.0.0.1", server->port, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to create server address: %s", err.message);
        goto cleanup;
    }
    
    size_t sent;
    err = client->sendto(client, "x", 0, &sent, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to send zero bytes: %s", err.message);
        goto cleanup;
    }
    if (sent != 0) {
        TEST_LOG_ERROR("Expected to send 0 bytes, but sent %zu", sent);
        goto cleanup;
    }
    TEST_LOG_INFO("Zero bytes send test passed");
    
    // Test 2: Send maximum size packet
    char* packet_buffer = malloc(UDP_MAX_PACKET_SIZE);
    if (!packet_buffer) {
        TEST_LOG_ERROR("Failed to allocate packet buffer");
        goto cleanup;
    }
    memset(packet_buffer, 'A', UDP_MAX_PACKET_SIZE);
    
    err = client->sendto(client, packet_buffer, UDP_MAX_PACKET_SIZE, &sent, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to send packet: %s", err.message);
        free(packet_buffer);
        goto cleanup;
    }
    if (sent != UDP_MAX_PACKET_SIZE) {
        TEST_LOG_ERROR("Failed to send entire packet: sent %zu of %d", sent, UDP_MAX_PACKET_SIZE);
        free(packet_buffer);
        goto cleanup;
    }
    free(packet_buffer);
    TEST_LOG_INFO("Maximum packet size test passed");
    
    // Test 3: Large file transfer (512KB)
    size_t large_file_size = 512 * 1024;
    char* large_file_buffer = malloc(large_file_size);
    if (!large_file_buffer) {
        TEST_LOG_ERROR("Failed to allocate large file buffer");
        goto cleanup;
    }

    // Fill buffer with pattern data
    for (size_t i = 0; i < large_file_size; i++) {
        large_file_buffer[i] = 'A' + (i % 26);
    }

    // Send large file in chunks
    size_t total_sent = 0;
    size_t last_progress = 0;
    while (total_sent < large_file_size) {
        size_t chunk_size = large_file_size - total_sent;
        if (chunk_size > UDP_MAX_PACKET_SIZE) {
            chunk_size = UDP_MAX_PACKET_SIZE;
        }

        err = client->sendto(client, large_file_buffer + total_sent, chunk_size, &sent, &server_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_ERROR("Failed to send large file chunk: %s", err.message);
            free(large_file_buffer);
            goto cleanup;
        }
        if (sent == 0) {
            TEST_LOG_ERROR("Failed to send large file: connection closed");
            free(large_file_buffer);
            goto cleanup;
        }
        
        total_sent += sent;
        
        // 流控
        if (sent >= FLOW_CONTROL_CHUNK_SIZE) {
            core->sleep_ms(core, FLOW_CONTROL_DELAY_MS);
        }
        
        // 更新进度
        if (total_sent - last_progress >= PROGRESS_UPDATE_INTERVAL) {
            TEST_LOG_INFO("Sent %zu bytes of large file (%zu%%)", total_sent, (total_sent * 100) / large_file_size);
            last_progress = total_sent;
        }
    }

    free(large_file_buffer);
    TEST_LOG_INFO("Large file transfer test passed");
    
    success = true;

cleanup:
    if (client) InfraxSocketClass.free(client);
    return success;
}

// Test runner
static bool run_test_suite(TestSuite* suite) {
    bool all_passed = true;
    
    TEST_LOG_INFO("Running test suite: %s", suite->name);
    
    for (size_t i = 0; i < suite->case_count; i++) {
        TestCase* test = &suite->cases[i];
        TEST_LOG_INFO("Running test case: %s", test->name);
        
        // Create server context if needed
        ServerContext* ctx = NULL;
        if (test->setup) {
            ctx = create_server_context(strstr(test->name, "udp") != NULL,
                                      TEST_PORT_BASE + i);
            if (!ctx) {
                TEST_LOG_ERROR("Failed to create server context");
                all_passed = false;
                continue;
            }
            test->arg = ctx;
        }
        
        // Setup
        if (test->setup && !test->setup(test->arg)) {
            TEST_LOG_ERROR("Test setup failed");
            destroy_server_context(ctx);
            all_passed = false;
            continue;
        }
        
        // Run test
        bool passed = test->run(test->arg);
        
        // Cleanup
        if (test->cleanup) {
            test->cleanup(test->arg);
        }
        
        if (ctx) {
            destroy_server_context(ctx);
        }
        
        if (!passed) {
            TEST_LOG_ERROR("Test case failed: %s", test->name);
            all_passed = false;
        } else {
            TEST_LOG_INFO("Test case passed: %s", test->name);
        }
    }
    TEST_LOG_INFO("return all_passed: %s", all_passed ? "true" : "false");
    
    return all_passed;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // 初始化core
    core = InfraxCoreClass.singleton();
    if (!core) {
        TEST_LOG_ERROR("Failed to initialize core");
        return 1;
    }
    
    // Initialize test suites
    TestSuite suites[] = {
        {
            .name = "error_handling",
            .cases = (TestCase[]){
                {"invalid_address", NULL, test_invalid_address, NULL, TEST_TIMEOUT_SEC * 1000, NULL},
                {"port_in_use", NULL, test_port_in_use, NULL, TEST_TIMEOUT_SEC * 1000, NULL},
                {"connection_timeout", NULL, test_connection_timeout, NULL, TEST_TIMEOUT_SEC * 1000, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 3,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "boundary_conditions",
            .cases = (TestCase[]){
                {"tcp_boundary", setup_tcp_server, test_tcp_boundary, NULL, TEST_TIMEOUT_SEC * 1000, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "udp_boundary",
            .cases = (TestCase[]){
                {"udp_boundary", setup_udp_server, test_udp_boundary, NULL, TEST_TIMEOUT_SEC * 1000, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "basic_functionality",
            .cases = (TestCase[]){
                {"tcp_basic", setup_tcp_server, test_tcp_basic, NULL, TEST_TIMEOUT_SEC * 1000, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "udp_functionality",
            .cases = (TestCase[]){
                {"udp_basic", setup_udp_server, test_udp_basic, NULL, TEST_TIMEOUT_SEC * 1000, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {NULL, NULL, 0, NULL, NULL}  // 结束标记
    };
    
    // Run all test suites
    bool all_passed = true;
    for (TestSuite* suite = suites; suite->name != NULL; suite++) {
        all_passed &= run_test_suite(suite);
    }
    
    // Reset core instance
    core = NULL;
    
    return all_passed ? 0 : 1;
}
