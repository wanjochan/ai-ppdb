#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxSync.h"
#include <string.h>
#include <stdlib.h>


// Test parameters
#define TEST_PORT_BASE 22345
#define TEST_TIMEOUT_MS 5000
#define TEST_BUFFER_SIZE 4096
#define TEST_RETRY_COUNT 5
#define TEST_RETRY_DELAY_MS 500

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
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS,
        .reuse_addr = true
    };
    
    ctx->socket = InfraxSocketClass.new(&config);
    if (!ctx->socket) {
        TEST_LOG_ERROR("Failed to create socket");
        goto error;
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
    
    if (ctx->thread) {
        void* thread_result;
        ctx->thread->join(ctx->thread, &thread_result);
        InfraxThreadClass.free(ctx->thread);
    }
    
    if (ctx->socket) InfraxSocketClass.free(ctx->socket);
    if (ctx->ready_mutex) InfraxSyncClass.free(ctx->ready_mutex);
    if (ctx->ready_cond) InfraxSyncClass.free(ctx->ready_cond);
    
    free(ctx);
}

// TCP server implementation
static void* tcp_server_thread(void* arg) {
    ServerContext* ctx = (ServerContext*)arg;
    InfraxError err;
    InfraxSocket* client = NULL;
    char buffer[TEST_BUFFER_SIZE];
    
    // Bind and listen
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
    
    err = ctx->socket->listen(ctx->socket, 5);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to listen: %s", err.message);
        return NULL;
    }
    
    // Signal ready
    ctx->ready_mutex->mutex_lock(ctx->ready_mutex);
    ctx->is_ready = true;
    ctx->ready_cond->cond_signal(ctx->ready_cond);
    ctx->ready_mutex->mutex_unlock(ctx->ready_mutex);
    
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
        
        TEST_LOG_INFO("Client connected from %s:%d", client_addr.ip, client_addr.port);
        
        // Echo loop
        while (ctx->is_running) {
            size_t received;
            err = client->recv(client, buffer, sizeof(buffer), &received);
            if (INFRAX_ERROR_IS_ERR(err)) {
                TEST_LOG_ERROR("Receive error: %s", err.message);
                break;
            }
            
            if (received == 0) {
                TEST_LOG_INFO("Client disconnected");
                break;
            }
            
            size_t sent;
            err = client->send(client, buffer, received, &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                TEST_LOG_ERROR("Send error: %s", err.message);
                break;
            }
            
            ctx->stats.total_bytes += sent;
            ctx->stats.total_packets++;
        }
        
        if (client) {
            InfraxSocketClass.free(client);
            client = NULL;
        }
    }
    
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
    ctx->ready_mutex->mutex_lock(ctx->ready_mutex);
    ctx->is_ready = true;
    ctx->ready_cond->cond_signal(ctx->ready_cond);
    ctx->ready_mutex->mutex_unlock(ctx->ready_mutex);
    
    TEST_LOG_INFO("UDP server ready on port %d", ctx->port);
    
    // Main loop
    while (ctx->is_running) {
        InfraxNetAddr client_addr;
        size_t received;
        err = ctx->socket->recvfrom(ctx->socket, buffer, sizeof(buffer), &received, &client_addr);
        
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                continue;
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
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS
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
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS
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
    err = ctx->thread->start(ctx->thread, tcp_server_thread, ctx);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to start server thread: %s", err.message);
        return false;
    }
    
    // Wait for ready
    int retry = TEST_RETRY_COUNT;
    while (retry > 0 && !ctx->is_ready) {
        ctx->ready_mutex->mutex_lock(ctx->ready_mutex);
        err = ctx->ready_cond->cond_timedwait(ctx->ready_cond, 
                                            ctx->ready_mutex, 
                                            TEST_TIMEOUT_MS);
        ctx->ready_mutex->mutex_unlock(ctx->ready_mutex);
        
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
    err = ctx->thread->start(ctx->thread, udp_server_thread, ctx);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to start server thread: %s", err.message);
        return false;
    }
    
    // Wait for ready
    int retry = TEST_RETRY_COUNT;
    while (retry > 0 && !ctx->is_ready) {
        ctx->ready_mutex->mutex_lock(ctx->ready_mutex);
        err = ctx->ready_cond->cond_timedwait(ctx->ready_cond, 
                                            ctx->ready_mutex, 
                                            TEST_TIMEOUT_MS);
        ctx->ready_mutex->mutex_unlock(ctx->ready_mutex);
        
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
        .timeout_ms = TEST_TIMEOUT_MS
    },
    {
        .name = "udp_basic",
        .setup = setup_udp_server,
        .run = test_udp_basic,
        .cleanup = NULL,
        .timeout_ms = TEST_TIMEOUT_MS
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
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS
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
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS,
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
    
    // Create client
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = false,
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS
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
    size_t sent;
    err = client->send(client, "x", 0, &sent);
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
    char* large_buffer = malloc(TEST_BUFFER_SIZE);
    if (!large_buffer) {
        TEST_LOG_ERROR("Failed to allocate large buffer");
        goto cleanup;
    }
    memset(large_buffer, 'A', TEST_BUFFER_SIZE);
    
    err = client->send(client, large_buffer, TEST_BUFFER_SIZE, &sent);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to send large buffer: %s", err.message);
        free(large_buffer);
        goto cleanup;
    }
    if (sent != TEST_BUFFER_SIZE) {
        TEST_LOG_ERROR("Failed to send entire large buffer: sent %zu of %zu", sent, TEST_BUFFER_SIZE);
        free(large_buffer);
        goto cleanup;
    }
    free(large_buffer);
    TEST_LOG_INFO("Large buffer send test passed");
    
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
        .send_timeout_ms = TEST_TIMEOUT_MS,
        .recv_timeout_ms = TEST_TIMEOUT_MS
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
    char* large_buffer = malloc(TEST_BUFFER_SIZE);
    if (!large_buffer) {
        TEST_LOG_ERROR("Failed to allocate large buffer");
        goto cleanup;
    }
    memset(large_buffer, 'A', TEST_BUFFER_SIZE);
    
    err = client->sendto(client, large_buffer, TEST_BUFFER_SIZE, &sent, &server_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to send large buffer: %s", err.message);
        free(large_buffer);
        goto cleanup;
    }
    if (sent != TEST_BUFFER_SIZE) {
        TEST_LOG_ERROR("Failed to send entire large buffer: sent %zu of %zu", sent, TEST_BUFFER_SIZE);
        free(large_buffer);
        goto cleanup;
    }
    free(large_buffer);
    TEST_LOG_INFO("Large buffer send test passed");
    
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
                {"invalid_address", NULL, test_invalid_address, NULL, TEST_TIMEOUT_MS, NULL},
                {"port_in_use", NULL, test_port_in_use, NULL, TEST_TIMEOUT_MS, NULL},
                {"connection_timeout", NULL, test_connection_timeout, NULL, TEST_TIMEOUT_MS, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 3,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "boundary_conditions",
            .cases = (TestCase[]){
                {"tcp_boundary", setup_tcp_server, test_tcp_boundary, NULL, TEST_TIMEOUT_MS, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "udp_boundary",
            .cases = (TestCase[]){
                {"udp_boundary", setup_udp_server, test_udp_boundary, NULL, TEST_TIMEOUT_MS, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "basic_functionality",
            .cases = (TestCase[]){
                {"tcp_basic", setup_tcp_server, test_tcp_basic, NULL, TEST_TIMEOUT_MS, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "udp_functionality",
            .cases = (TestCase[]){
                {"udp_basic", setup_udp_server, test_udp_basic, NULL, TEST_TIMEOUT_MS, NULL},
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
    
    return all_passed ? 0 : 1;
}
