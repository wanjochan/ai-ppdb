#include "ppx/infrax/InfraxNet.h"
#include "ppx/infrax/InfraxCore.h"
#include "ppx/polyx/PolyxAsync.h"

// Test parameters
#define TEST_PORT_TCP 22345
#define TEST_PORT_UDP 22346
#define TEST_TIMEOUT_MS 5000
#define TEST_BUFFER_SIZE 4096
#define TEST_MESSAGE "Hello, World!"
#define TEST_MAX_RETRIES 3
#define TEST_RETRY_DELAY_MS 100

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
    bool is_server;
    bool data_sent;
    bool data_received;
    char buffer[TEST_BUFFER_SIZE];
    PolyxAsync* async;
} TestContext;

// Global variables
static PolyxAsync* async = NULL;
static bool server_running = false;

// Forward declarations
static void tcp_server_handler(PolyxEvent* event, void* arg);
static void tcp_poll_handler(PolyxAsync* async, int fd, InfraxPollEvents events, void* arg);
static void tcp_client_handler(PolyxAsync* async, int fd, InfraxPollEvents events, void* arg);
static void udp_server_handler(PolyxAsync* async, int fd, InfraxPollEvents events, void* arg);
static InfraxError send_with_retry(PolyxAsync* async, InfraxSocket* socket, const void* data, size_t size, size_t* sent);
static InfraxError recv_with_retry(PolyxAsync* async, InfraxSocket* socket, void* buffer, size_t size, size_t* received);

// Initialize test environment
static void init_test(void) {
    // Create async instance
    async = PolyxAsyncClass.new();
    if (!async) {
        TEST_LOG_ERROR("Failed to create async instance");
        return;
    }
}

// Cleanup test environment
static void cleanup_test(void) {
    if (async) {
        PolyxAsyncClass.free(async);
        async = NULL;
    }
}

// Poll handler for TCP server
static void tcp_poll_handler(PolyxAsync* async, int fd, InfraxPollEvents events, void* arg) {
    TestContext* ctx = (TestContext*)arg;
    InfraxSocket* client_socket = NULL;
    InfraxNetAddr client_addr;
    InfraxError err;

    // Accept new client connection
    err = ctx->socket->accept(ctx->socket, &client_socket, &client_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to accept client connection: %s", err.message);
        return;
    }

    // Create context for client
    TestContext* client_ctx = (TestContext*)malloc(sizeof(TestContext));
    if (!client_ctx) {
        TEST_LOG_ERROR("Failed to allocate client context");
        InfraxSocketClass.free(client_socket);
        return;
    }

    memset(client_ctx, 0, sizeof(TestContext));
    client_ctx->socket = client_socket;
    client_ctx->addr = client_addr;
    client_ctx->is_server = false;
    client_ctx->async = ctx->async;

    // Add client socket to pollset
    int ret = async->klass->pollset_add_fd(async, client_socket->native_handle, 
        INFRAX_POLL_IN | INFRAX_POLL_OUT, tcp_client_handler, client_ctx);
    if (ret < 0) {
        TEST_LOG_ERROR("Failed to add client socket to pollset");
        InfraxSocketClass.free(client_socket);
        free(client_ctx);
        return;
    }
}

// Poll handler for TCP client
static void tcp_client_handler(PolyxAsync* async, int fd, InfraxPollEvents events, void* arg) {
    TestContext* ctx = (TestContext*)arg;
    static int retry_count = 0;
    InfraxError err;
    size_t sent = 0;
    size_t received = 0;
    const char* message = TEST_MESSAGE;

    if (!ctx->data_sent) {
        if (retry_count >= TEST_MAX_RETRIES) {
            TEST_LOG_ERROR("Max retries reached for sending data");
            goto cleanup;
        }
        // Send test message
        err = send_with_retry(ctx->async, ctx->socket, message, strlen(message), &sent);
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_ERROR("Failed to send message: %s", err.message);
            goto cleanup;
        }

        if (sent != strlen(message)) {
            TEST_LOG_ERROR("Failed to send complete message");
            goto cleanup;
        }

        ctx->data_sent = true;
        TEST_LOG_INFO("Sent message: %s", message);
        retry_count++;
    } else if (!ctx->data_received) {
        // Receive response
        err = recv_with_retry(ctx->async, ctx->socket, ctx->buffer, TEST_BUFFER_SIZE, &received);
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_ERROR("Failed to receive message: %s", err.message);
            goto cleanup;
        }

        if (received > 0) {
            ctx->buffer[received] = '\0';
            TEST_LOG_INFO("Received message: %s", ctx->buffer);

            // Echo back the received message
            err = send_with_retry(ctx->async, ctx->socket, ctx->buffer, received, &sent);
            if (INFRAX_ERROR_IS_ERR(err)) {
                TEST_LOG_ERROR("Failed to echo message: %s", err.message);
                goto cleanup;
            }

            ctx->data_received = true;
        }
    }

    return;

cleanup:
    async->klass->pollset_remove_fd(async, fd);
    InfraxSocketClass.free(ctx->socket);
    free(ctx);
}

// UDP server handler
static void udp_server_handler(PolyxAsync* async, int fd, InfraxPollEvents events, void* arg) {
    InfraxError err;
    TestContext* ctx = (TestContext*)arg;
    InfraxNetAddr client_addr;
    size_t received, sent;

    // Receive data
    err = ctx->socket->recvfrom(ctx->socket, ctx->buffer, TEST_BUFFER_SIZE, &received, &client_addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        if (err.code != INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
            TEST_LOG_ERROR("UDP server: Receive failed: %s", err.message);
        }
        return;
    }

    if (received > 0) {
        // Echo back
        err = ctx->socket->sendto(ctx->socket, ctx->buffer, received, &sent, &client_addr);
        if (INFRAX_ERROR_IS_ERR(err)) {
            TEST_LOG_ERROR("UDP server: Send failed: %s", err.message);
        }
    }
}

// Send with retry
static InfraxError send_with_retry(PolyxAsync* async, InfraxSocket* socket, const void* data, size_t size, size_t* sent) {
    InfraxError err = {0};
    *sent = 0;

    while (*sent < size) {
        size_t bytes_sent = 0;
        err = socket->send(socket, (char*)data + *sent, size - *sent, &bytes_sent);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                // Wait a bit and retry
                async->poll(async, 10);
                continue;
            }
            return err;
        }

        if (bytes_sent == 0) {
            err.code = INFRAX_ERROR_NET_TIMEOUT_CODE;
            strcpy(err.message, "Connection closed");
            return err;
        }

        *sent += bytes_sent;
    }

    err.code = INFRAX_ERROR_OK;
    strcpy(err.message, "Success");
    return err;
}

// Receive with retry
static InfraxError recv_with_retry(PolyxAsync* async, InfraxSocket* socket, void* buffer, size_t size, size_t* received) {
    InfraxError err = {0};
    *received = 0;

    while (*received < size) {
        size_t bytes_received = 0;
        err = socket->recv(socket, (char*)buffer + *received, size - *received, &bytes_received);
        if (INFRAX_ERROR_IS_ERR(err)) {
            if (err.code == INFRAX_ERROR_NET_WOULD_BLOCK_CODE) {
                // Wait a bit and retry
                async->poll(async, 10);
                continue;
            }
            return err;
        }

        if (bytes_received == 0) {
            err.code = INFRAX_ERROR_NET_TIMEOUT_CODE;
            strcpy(err.message, "Connection closed");
            return err;
        }

        *received += bytes_received;
        break;  // For TCP, we don't need to receive the full buffer
    }

    err.code = INFRAX_ERROR_OK;
    strcpy(err.message, "Success");
    return err;
}

// Test UDP functionality
bool test_udp(void) {
    InfraxError err;
    TestContext server_ctx = {0};
    TestContext client_ctx = {0};
    bool success = false;

    // Initialize server context
    server_ctx.is_server = true;
    server_ctx.async = async;

    // Create server socket
    InfraxSocketConfig config = {
        .is_udp = true,
        .is_nonblocking = true,
        .reuse_addr = true
    };
    server_ctx.socket = InfraxSocketClass.new(&config);
    if (!server_ctx.socket) {
        TEST_LOG_ERROR("Failed to create server socket");
        return false;
    }

    // Bind server socket
    server_ctx.addr.port = TEST_PORT_UDP;
    strcpy(server_ctx.addr.ip, "127.0.0.1");
    err = server_ctx.socket->bind(server_ctx.socket, &server_ctx.addr);
    if (INFRAX_ERROR_IS_ERR(err)) {
        TEST_LOG_ERROR("Failed to bind server socket: %s", err.message);
        goto cleanup;
    }

    // Add server socket to pollset
    int ret = async->infrax->klass->pollset_add_fd(async->infrax, server_ctx.socket->native_handle,
        INFRAX_POLL_IN, udp_server_handler, &server_ctx);
    if (ret < 0) {
        TEST_LOG_ERROR("Failed to add server socket to pollset");
        goto cleanup;
    }

    server_running = true;

    // Run event loop
    while (server_running) {
        async->infrax->klass->pollset_poll(async->infrax, TEST_TIMEOUT_MS);
    }

    success = true;

cleanup:
    if (server_ctx.socket) {
        InfraxSocketClass.free(server_ctx.socket);
    }

    return success;
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
    int server_fd = 0;
    int client_fd = 0;

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
    int ret = async->infrax->klass->pollset_add_fd(async->infrax, server_fd, INFRAX_POLL_IN, tcp_poll_handler, &server_ctx);
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
    ret = async->infrax->klass->pollset_add_fd(async->infrax, client_fd, INFRAX_POLL_IN | INFRAX_POLL_OUT, tcp_client_handler, &client_ctx);
    if (ret < 0) {
        TEST_LOG_ERROR("Failed to add client socket to pollset");
        success = false;
        goto cleanup_tcp;
    }

    // Wait for client to complete data exchange
    for (int i = 0; i < 1000 && !client_ctx.data_received; i++) {
        async->infrax->klass->pollset_poll(async->infrax, 10);
    }

    // Check if data exchange was successful
    if (!client_ctx.data_received) {
        TEST_LOG_ERROR("Data exchange failed");
        success = false;
        goto cleanup_tcp;
    }

cleanup_tcp:
    TEST_LOG_INFO("Cleaning up TCP test resources...");
    if (client_fd) {
        async->infrax->klass->pollset_remove_fd(async->infrax, client_fd);
    }
    if (server_fd) {
        async->infrax->klass->pollset_remove_fd(async->infrax, server_fd);
    }
    if (server_ctx.socket) InfraxSocketClass.free(server_ctx.socket);
    if (client_ctx.socket) InfraxSocketClass.free(client_ctx.socket);
    TEST_LOG_INFO("TCP test cleanup completed");
    return success;
}

// Main function
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Initialize core
    InfraxCore* core = InfraxCoreClass.singleton();
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
