#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/polyx/PolyxAsync.h"
#include <string.h>
#include <stdlib.h>

// Test parameters
#define TEST_PORT_TCP 22345
#define TEST_PORT_UDP 22346
#define TEST_TIMEOUT_MS 5000
#define TEST_BUFFER_SIZE 4096
#define TEST_MESSAGE "Hello, World!"

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

// Test case structure
typedef struct {
    const char* name;
    void (*setup)(void);
    bool (*test)(void);
    void (*cleanup)(void);
    uint32_t timeout_ms;
    void* arg;
} TestCase;

// Test suite structure
typedef struct {
    const char* name;
    TestCase* cases;
    size_t case_count;
    void (*before_all)(void);
    void (*after_all)(void);
} TestSuite;

// Test context structure
typedef struct {
    InfraxSocket* socket;
    InfraxNetAddr addr;
    char buffer[TEST_BUFFER_SIZE];
    bool is_server;
    bool is_udp;
    bool connected;
    bool data_sent;
    bool data_received;
} TestContext;

// Global variables
static InfraxCore* core = NULL;
static PolyxAsync* async = NULL;
static volatile bool server_running = false;

// Forward declarations
static InfraxError send_with_retry(InfraxSocket* socket, const void* data, size_t size, size_t* sent);
static InfraxError recv_with_retry(InfraxSocket* socket, void* buffer, size_t size, size_t* received);
static void tcp_server_handler(PolyxEvent* event, void* arg);
static void tcp_poll_handler(InfraxAsync* async, InfraxHandle fd, short events, void* arg);
static void tcp_client_handler(InfraxAsync* async, InfraxHandle fd, short events, void* arg);
static void udp_server_handler(InfraxAsync* self, void* arg);

// Initialize test environment
static void init_test(void) {
    core = InfraxCoreClass.singleton();
    async = PolyxAsyncClass.new();
    if (!async) {
        core->printf(core, "Failed to create async instance\n");
        exit(1);
    }
    if (!async->infrax) {
        core->printf(core, "Failed to create infrax instance\n");
        exit(1);
    }
}

// Cleanup test environment
static void cleanup_test(void) {
    if (async) {
        PolyxAsyncClass.free(async);
        async = NULL;
    }
}

// Async TCP server handler
static void tcp_server_handler(PolyxEvent* event, void* arg) {
    InfraxError err;
    TestContext* ctx = (TestContext*)arg;
    InfraxSocket* client_socket = NULL;
    InfraxNetAddr client_addr = {0};

    // Accept new connection
    err = ctx->socket->accept(ctx->socket, &client_socket, &client_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
            return;
        }
        core->printf(core, "Failed to accept: %s\n", err.message);
        return;
    }

    if (client_socket) {
        // Set client socket to non-blocking mode
        client_socket->set_nonblock(client_socket, true);

        // Echo received data
        char buffer[TEST_BUFFER_SIZE];
        size_t received;
        err = recv_with_retry(client_socket, buffer, TEST_BUFFER_SIZE, &received);
        if (!INFRAX_ERROR_IS_ERR(err) && received > 0) {
            size_t sent;
            err = send_with_retry(client_socket, buffer, received, &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                core->printf(core, "Failed to send echo: %s\n", err.message);
            }
        }

        // Close client socket
        InfraxSocketClass.free(client_socket);
    }
}

// Poll handler for TCP server
static void tcp_poll_handler(InfraxAsync* async, InfraxHandle fd, short events, void* arg) {
    InfraxError err;
    TestContext* ctx = (TestContext*)arg;
    InfraxNetAddr client_addr;
    InfraxSocket* client_socket = NULL;

    // Accept new connection
    err = ctx->socket->accept(ctx->socket, &client_socket, &client_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to accept: %s", err.message);
        return;
    }

    TEST_LOG_INFO("Server: accepted new connection");

    // Set client socket configuration
    InfraxSocketConfig client_config = {
        .is_udp = false,
        .is_nonblocking = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };

    // Create client context
    TestContext* client_ctx = (TestContext*)malloc(sizeof(TestContext));
    if (!client_ctx) {
        TEST_LOG_ERROR("Failed to allocate client context");
        InfraxSocketClass.free(client_socket);
        return;
    }

    // Initialize client context
    memset(client_ctx, 0, sizeof(TestContext));
    client_ctx->socket = client_socket;
    client_ctx->is_server = true;
    client_ctx->addr = client_addr;

    // Add client socket to pollset
    int ret = InfraxAsyncClass.pollset_add_fd(async->infrax, client_socket->native_handle, INFRAX_POLLIN, tcp_client_handler, client_ctx);
    if (ret < 0) {
        TEST_LOG_ERROR("Failed to add client socket to pollset");
        InfraxSocketClass.free(client_socket);
        free(client_ctx);
        return;
    }
}

// Poll handler for TCP client
static void tcp_client_handler(InfraxAsync* async, InfraxHandle fd, short events, void* arg) {
    InfraxError err;
    TestContext* ctx = (TestContext*)arg;
    size_t sent, received;

    // Handle write events
    if ((events & INFRAX_POLLOUT) && !ctx->data_sent) {
        const char* message = TEST_MESSAGE;
        err = send_with_retry(ctx->socket, message, strlen(message), &sent);
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_ERROR("Failed to send: %s", err.message);
            goto cleanup;
        }

        TEST_LOG_INFO("Client: sent %zu bytes", sent);
        ctx->data_sent = true;
    }

    // Handle read events
    if (events & INFRAX_POLLIN) {
        err = recv_with_retry(ctx->socket, ctx->buffer, TEST_BUFFER_SIZE, &received);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code != INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                TEST_LOG_ERROR("Failed to receive: %s", err.message);
                goto cleanup;
            }
            return;
        }

        if (received > 0) {
            TEST_LOG_INFO("Client: received %zu bytes", received);
            if (ctx->is_server) {
                // Echo back the data
                err = send_with_retry(ctx->socket, ctx->buffer, received, &sent);
                if (INFRAX_ERROR_IS_ERR(err)) {
                    TEST_LOG_ERROR("Failed to send echo: %s", err.message);
                    goto cleanup;
                }
                TEST_LOG_INFO("Server: echoed %zu bytes", sent);
            } else {
                // Verify received data
                if (received == strlen(TEST_MESSAGE) && memcmp(ctx->buffer, TEST_MESSAGE, received) == 0) {
                    TEST_LOG_INFO("Client: data verified");
                    ctx->data_received = true;
                }
            }
        }
    }

    return;

cleanup:
    InfraxAsyncClass.pollset_remove_fd(async->infrax, fd);
    InfraxSocketClass.free(ctx->socket);
    if (!ctx->is_server) {
        free(ctx);
    }
}

// Async UDP server handler
static void udp_server_handler(InfraxAsync* self, void* arg) {
    TestContext* ctx = (TestContext*)arg;
    InfraxError err;
    InfraxNetAddr client_addr;

    while (server_running) {
        // Receive data
        size_t received;
        err = ctx->socket->recvfrom(ctx->socket, ctx->buffer, TEST_BUFFER_SIZE, &received, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "UDP server: Receive failed: %s\n", err.message);
            continue;
        }

        // Echo back
        size_t sent;
        err = ctx->socket->sendto(ctx->socket, ctx->buffer, received, &sent, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            core->printf(core, "UDP server: Send failed: %s\n", err.message);
        }

        // Give other tasks a chance to run
        InfraxAsyncClass.pollset_poll(async->infrax, 0);
    }
}

// Send data with retry
static InfraxError send_with_retry(InfraxSocket* socket, const void* data, size_t size, size_t* sent) {
    InfraxError err;
    size_t total_sent = 0;
    const char* ptr = (const char*)data;

    while (total_sent < size) {
        size_t current_sent = 0;
        err = socket->send(socket, ptr + total_sent, size - total_sent, &current_sent);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                // Wait a bit and retry
                InfraxAsyncClass.pollset_poll(async->infrax, 10);
                continue;
            }
            return err;
        }
        total_sent += current_sent;
    }

    if (sent) *sent = total_sent;
    return INFRAX_ERROR_OK_STRUCT;
}

// Receive data with retry
static InfraxError recv_with_retry(InfraxSocket* socket, void* buffer, size_t size, size_t* received) {
    InfraxError err;
    size_t total_received = 0;
    char* ptr = (char*)buffer;

    while (total_received < size) {
        size_t current_received = 0;
        err = socket->recv(socket, ptr + total_received, size - total_received, &current_received);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                // Wait a bit and retry
                InfraxAsyncClass.pollset_poll(async->infrax, 10);
                continue;
            }
            return err;
        }
        if (current_received == 0) {
            // Connection closed
            break;
        }
        total_received += current_received;
    }

    if (received) *received = total_received;
    return INFRAX_ERROR_OK_STRUCT;
}

// Run test suite
static bool run_test_suite(TestSuite* suite) {
    bool success = true;
    
    TEST_LOG_INFO("Running test suite: %s", suite->name);
    
    // Run before_all if present
    if (suite->before_all) {
        suite->before_all();
    }
    
    // Run each test case
    for (size_t i = 0; i < suite->case_count; i++) {
        TestCase* test_case = &suite->cases[i];
        if (!test_case->name) break;
        
        TEST_LOG_INFO("Running test case: %s", test_case->name);
        
        // Run setup if present
        if (test_case->setup) {
            test_case->setup();
        }
        
        // Run test with timeout
        bool test_success = test_case->test();
        if (!test_success) {
            TEST_LOG_ERROR("Test case failed: %s", test_case->name);
            success = false;
        }
        
        // Run cleanup if present
        if (test_case->cleanup) {
            test_case->cleanup();
        }
    }
    
    // Run after_all if present
    if (suite->after_all) {
        suite->after_all();
    }
    
    return success;
}

// Run all test suites
static int run_test_suites(TestSuite* suites) {
    bool success = true;
    
    // Run each test suite
    for (TestSuite* suite = suites; suite->name != NULL; suite++) {
        if (!run_test_suite(suite)) {
            success = false;
        }
    }
    
    return success ? 0 : 1;
}

// Test TCP functionality
static bool test_tcp(void) {
    InfraxError err;
    bool success = true;
    TestContext server_ctx = {0};
    TestContext client_ctx = {0};
    InfraxHandle server_fd = 0;
    InfraxHandle client_fd = 0;

    // Create and setup server socket
    InfraxSocketConfig server_config = {
        .is_udp = false,
        .is_nonblocking = true,
        .reuse_addr = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    server_ctx.socket = InfraxSocketClass.new(&server_config);
    if (!server_ctx.socket) {
        TEST_LOG_ERROR("Failed to create server socket");
        success = false;
        goto cleanup_tcp;
    }

    // Bind server socket
    server_ctx.addr.port = TEST_PORT_TCP;
    strcpy(server_ctx.addr.ip, "127.0.0.1");
    err = server_ctx.socket->bind(server_ctx.socket, &server_ctx.addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to bind server socket: %s", err.message);
        success = false;
        goto cleanup_tcp;
    }

    // Listen
    err = server_ctx.socket->listen(server_ctx.socket, 5);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to listen: %s", err.message);
        success = false;
        goto cleanup_tcp;
    }

    // Get server socket file descriptor
    server_fd = server_ctx.socket->native_handle;
    if (server_fd == 0) {
        TEST_LOG_ERROR("Failed to get server socket handle");
        success = false;
        goto cleanup_tcp;
    }

    // Add server socket to pollset
    int ret = InfraxAsyncClass.pollset_add_fd(async->infrax, server_fd, INFRAX_POLLIN, tcp_poll_handler, &server_ctx);
    if (ret < 0) {
        TEST_LOG_ERROR("Failed to add server socket to pollset");
        success = false;
        goto cleanup_tcp;
    }

    // Create and setup client socket
    InfraxSocketConfig client_config = {
        .is_udp = false,
        .is_nonblocking = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    client_ctx.socket = InfraxSocketClass.new(&client_config);
    if (!client_ctx.socket) {
        TEST_LOG_ERROR("Failed to create client socket");
        success = false;
        goto cleanup_tcp;
    }

    // Connect to server
    client_ctx.addr.port = TEST_PORT_TCP;
    strcpy(client_ctx.addr.ip, "127.0.0.1");
    err = client_ctx.socket->connect(client_ctx.socket, &client_ctx.addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        if (err.code != INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
            TEST_LOG_ERROR("Failed to connect: %s", err.message);
            success = false;
            goto cleanup_tcp;
        }
    }

    // Get client socket file descriptor
    client_fd = client_ctx.socket->native_handle;
    if (client_fd == 0) {
        TEST_LOG_ERROR("Failed to get client socket handle");
        success = false;
        goto cleanup_tcp;
    }

    // Add client socket to pollset
    ret = InfraxAsyncClass.pollset_add_fd(async->infrax, client_fd, INFRAX_POLLIN | INFRAX_POLLOUT, tcp_client_handler, &client_ctx);
    if (ret < 0) {
        TEST_LOG_ERROR("Failed to add client socket to pollset");
        success = false;
        goto cleanup_tcp;
    }

    // Wait for client to complete data exchange
    for (int i = 0; i < 1000 && !client_ctx.data_received; i++) {
        InfraxAsyncClass.pollset_poll(async->infrax, 10);
    }

    // Check if data exchange was successful
    if (!client_ctx.data_received) {
        TEST_LOG_ERROR("Data exchange failed");
        success = false;
        goto cleanup_tcp;
    }

cleanup_tcp:
    if (client_fd) {
        InfraxAsyncClass.pollset_remove_fd(async->infrax, client_fd);
    }
    if (server_fd) {
        InfraxAsyncClass.pollset_remove_fd(async->infrax, server_fd);
    }
    if (server_ctx.socket) InfraxSocketClass.free(server_ctx.socket);
    if (client_ctx.socket) InfraxSocketClass.free(client_ctx.socket);
    return success;
}

// Test UDP functionality
static bool test_udp(void) {
    InfraxError err;
    bool success = true;
    TestContext server_ctx = {0};
    TestContext client_ctx = {0};
    PolyxEventConfig event_config = {0};
    PolyxEvent* server_event = NULL;

    // Create and setup server socket
    InfraxSocketConfig server_config = {
        .is_udp = true,
        .is_nonblocking = true,
        .reuse_addr = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    server_ctx.socket = InfraxSocketClass.new(&server_config);
    if (!server_ctx.socket) {
        TEST_LOG_ERROR("Failed to create server socket");
        success = false;
        goto cleanup_udp;
    }

    // Bind server socket
    server_ctx.addr.port = TEST_PORT_UDP;
    strcpy(server_ctx.addr.ip, "127.0.0.1");
    err = server_ctx.socket->bind(server_ctx.socket, &server_ctx.addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to bind server socket: %s", err.message);
        success = false;
        goto cleanup_udp;
    }

    // Start server task
    server_ctx.is_server = true;
    server_ctx.is_udp = true;
    
    // Create server event
    event_config.type = POLYX_EVENT_IO;
    event_config.callback = (EventCallback)udp_server_handler;
    event_config.arg = &server_ctx;
    server_event = async->klass->create_event(async, &event_config);
    if (!server_event) {
        TEST_LOG_ERROR("Failed to create server event");
        success = false;
        goto cleanup_udp;
    }

    // Create and setup client socket
    InfraxSocketConfig client_config = {
        .is_udp = true,
        .is_nonblocking = true,
        .send_timeout_ms = 1000,
        .recv_timeout_ms = 1000
    };
    client_ctx.socket = InfraxSocketClass.new(&client_config);
    if (!client_ctx.socket) {
        TEST_LOG_ERROR("Failed to create client socket");
        success = false;
        goto cleanup_udp;
    }

    // Send data
    const char* message = TEST_MESSAGE;
    size_t sent;
    client_ctx.addr.port = TEST_PORT_UDP;
    strcpy(client_ctx.addr.ip, "127.0.0.1");
    err = client_ctx.socket->sendto(client_ctx.socket, message, strlen(message), &sent, &client_ctx.addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to send: %s", err.message);
        success = false;
        goto cleanup_udp;
    }

    // Wait for response
    char buffer[TEST_BUFFER_SIZE];
    size_t received;
    InfraxNetAddr server_addr;
    for (int i = 0; i < 100; i++) {
        err = client_ctx.socket->recvfrom(client_ctx.socket, buffer, sizeof(buffer), &received, &server_addr);
        if (!INFRAX_ERROR_IS_ERR(err)) {
            if (received == strlen(TEST_MESSAGE) && memcmp(buffer, TEST_MESSAGE, received) == 0) {
                success = true;
                break;
            }
        }
        InfraxAsyncClass.pollset_poll(async->infrax, 10);
    }

cleanup_udp:
    if (server_event) {
        server_running = false;
        async->klass->destroy_event(async, server_event);
    }
    if (server_ctx.socket) InfraxSocketClass.free(server_ctx.socket);
    if (client_ctx.socket) InfraxSocketClass.free(client_ctx.socket);
    return success;
}

// Main function
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Initialize core
    core = InfraxCoreClass.singleton();
    if (!core) {
        core->printf(core, "Failed to initialize core");
        return 1;
    }

    // Initialize test suites
    TestSuite suites[] = {
        {
            .name = "tcp_async",
            .cases = (TestCase[]){
                {"tcp_async", init_test, test_tcp, cleanup_test, TEST_TIMEOUT_MS, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {
            .name = "udp_async",
            .cases = (TestCase[]){
                {"udp_async", init_test, test_udp, cleanup_test, TEST_TIMEOUT_MS, NULL},
                {NULL, NULL, NULL, NULL, 0, NULL}
            },
            .case_count = 1,
            .before_all = NULL,
            .after_all = NULL
        },
        {NULL, NULL, 0, NULL, NULL}  // End marker
    };

    // Run all test suites
    int result = run_test_suites(suites);

    return result;
}
