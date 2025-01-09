#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_macros.h"
#include "test_net.h"

#define TEST_PORT 12345
#define TEST_HOST "127.0.0.1"
#define BUFFER_SIZE 1024
#define NUM_CLIENTS 4
#define NUM_MESSAGES 100

static void client_thread_func(void* arg);
static void handle_connection(ppdb_connection_t* conn, uint32_t events);

typedef struct {
    int thread_id;
    const char* host;
    uint16_t port;
} client_context_t;

// 基本服务器功能测试
int test_net_server_basic(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_net_config_t config = {
        .host = TEST_HOST,
        .port = TEST_PORT,
        .max_connections = 10,
        .io_threads = 2,
        .read_buffer_size = BUFFER_SIZE,
        .write_buffer_size = BUFFER_SIZE,
        .backlog = 5
    };

    // 创建服务器
    ASSERT_OK(ppdb_base_net_server_create(&server));
    ASSERT_NOT_NULL(server);

    // 启动服务器
    ASSERT_OK(ppdb_base_net_server_start(server));

    // 等待一段时间
    ppdb_base_sleep(100);

    // 停止服务器
    ASSERT_OK(ppdb_base_net_server_stop(server));
    ASSERT_OK(ppdb_base_net_server_destroy(server));

    return 0;
}

// 基本连接功能测试
int test_net_connection_basic(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_base_event_handler_t handler = {
        .fd = -1,
        .events = PPDB_EVENT_READ | PPDB_EVENT_WRITE,
        .callback = (void*)handle_connection
    };

    // 创建并启动服务器
    ASSERT_OK(ppdb_base_net_server_create(&server));
    ASSERT_OK(ppdb_base_net_server_start(server));

    // 创建客户端连接
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(client_fd >= 0);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TEST_PORT),
        .sin_addr.s_addr = inet_addr(TEST_HOST)
    };

    ASSERT_OK(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)));

    // 发送和接收数据
    const char* test_msg = "Hello, Server!";
    char buffer[BUFFER_SIZE];
    
    ASSERT_TRUE(send(client_fd, test_msg, strlen(test_msg), 0) > 0);
    ASSERT_TRUE(recv(client_fd, buffer, BUFFER_SIZE, 0) > 0);

    // 清理
    close(client_fd);
    ASSERT_OK(ppdb_base_net_server_stop(server));
    ASSERT_OK(ppdb_base_net_server_destroy(server));

    return 0;
}

// 协议处理测试
int test_net_protocol_basic(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_protocol_ops_t proto_ops = {
        .create = NULL,  // 使用默认协议
        .destroy = NULL,
        .on_data = NULL,
        .on_close = NULL
    };

    // 创建并配置服务器
    ASSERT_OK(ppdb_base_net_server_create(&server));
    
    // 设置协议处理器
    // TODO: 实现协议处理器的测试

    // 启动服务器
    ASSERT_OK(ppdb_base_net_server_start(server));

    // 等待一段时间
    ppdb_base_sleep(100);

    // 清理
    ASSERT_OK(ppdb_base_net_server_stop(server));
    ASSERT_OK(ppdb_base_net_server_destroy(server));

    return 0;
}

// 错误处理测试
int test_net_errors(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_net_config_t config = {0};

    // 测试空参数
    ASSERT_ERROR(ppdb_base_net_server_create(NULL));
    ASSERT_ERROR(ppdb_base_net_server_start(NULL));
    ASSERT_ERROR(ppdb_base_net_server_stop(NULL));
    ASSERT_ERROR(ppdb_base_net_server_destroy(NULL));

    // 测试无效配置
    ASSERT_OK(ppdb_base_net_server_create(&server));
    config.port = 0;  // 无效端口
    ASSERT_ERROR(ppdb_base_net_server_start(server));

    // 测试重复启动
    config.port = TEST_PORT;
    ASSERT_OK(ppdb_base_net_server_start(server));
    ASSERT_ERROR(ppdb_base_net_server_start(server));

    // 清理
    ASSERT_OK(ppdb_base_net_server_stop(server));
    ASSERT_OK(ppdb_base_net_server_destroy(server));

    return 0;
}

// 并发测试
int test_net_concurrent(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_base_thread_t* clients[NUM_CLIENTS];
    client_context_t contexts[NUM_CLIENTS];

    // 创建并启动服务器
    ASSERT_OK(ppdb_base_net_server_create(&server));
    ASSERT_OK(ppdb_base_net_server_start(server));

    // 创建多个客户端线程
    for (int i = 0; i < NUM_CLIENTS; i++) {
        contexts[i].thread_id = i;
        contexts[i].host = TEST_HOST;
        contexts[i].port = TEST_PORT;
        ASSERT_OK(ppdb_base_thread_create(&clients[i], client_thread_func, &contexts[i]));
    }

    // 等待客户端线程完成
    for (int i = 0; i < NUM_CLIENTS; i++) {
        ASSERT_OK(ppdb_base_thread_join(clients[i]));
        ASSERT_OK(ppdb_base_thread_destroy(clients[i]));
    }

    // 清理
    ASSERT_OK(ppdb_base_net_server_stop(server));
    ASSERT_OK(ppdb_base_net_server_destroy(server));

    return 0;
}

static void client_thread_func(void* arg) {
    client_context_t* ctx = (client_context_t*)arg;
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    char buffer[BUFFER_SIZE];

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ctx->port),
        .sin_addr.s_addr = inet_addr(ctx->host)
    };

    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        for (int i = 0; i < NUM_MESSAGES; i++) {
            snprintf(buffer, sizeof(buffer), "Message %d from client %d", i, ctx->thread_id);
            send(client_fd, buffer, strlen(buffer), 0);
            recv(client_fd, buffer, BUFFER_SIZE, 0);
        }
    }

    close(client_fd);
}

static void handle_connection(ppdb_connection_t* conn, uint32_t events) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes;

    if (events & PPDB_EVENT_READ) {
        bytes = recv(conn->fd, buffer, BUFFER_SIZE, 0);
        if (bytes > 0) {
            // 简单的回显服务
            send(conn->fd, buffer, bytes, 0);
        }
    }
}

// Test data
static const int TEST_PORT = 12345;
static const int NUM_CLIENTS = 100;
static const int NUM_THREADS = 4;
static _Atomic(int) active_connections = 0;

// Test basic connection operations
static int test_connection_basic(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_connection_t* conn = NULL;
    
    // Create server
    ASSERT_OK(ppdb_net_server_create(&server, TEST_PORT));
    
    // Create client socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(client_fd >= 0);
    
    // Connect to server
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_TRUE(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    
    // Accept connection
    ASSERT_OK(ppdb_net_server_accept(server, &conn));
    ASSERT_NOT_NULL(conn);
    
    // Check initial state
    ppdb_connection_state_t state;
    ASSERT_OK(ppdb_net_get_connection_state(conn, &state));
    ASSERT_EQ(state, PPDB_CONN_STATE_INIT);
    
    // Set timeout
    ASSERT_OK(ppdb_net_set_connection_timeout(conn, 1000));
    
    // Send data
    const char* test_data = "Hello, World!";
    ASSERT_TRUE(write(client_fd, test_data, strlen(test_data)) > 0);
    
    // Handle read event
    ASSERT_OK(handle_connection_event(conn));
    
    // Check statistics
    uint64_t bytes_received, bytes_sent;
    uint32_t request_count, error_count, uptime;
    ASSERT_OK(ppdb_net_get_connection_stats(conn, &bytes_received, &bytes_sent,
                                         &request_count, &error_count, &uptime));
    ASSERT_EQ(bytes_received, strlen(test_data));
    ASSERT_EQ(request_count, 1);
    ASSERT_EQ(error_count, 0);
    
    // Cleanup
    close(client_fd);
    ASSERT_OK(cleanup_connection(conn));
    ASSERT_OK(ppdb_net_server_destroy(server));
    return 0;
}

// Test timeout handling
static int test_connection_timeout(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_connection_t* conn = NULL;
    
    // Create server
    ASSERT_OK(ppdb_net_server_create(&server, TEST_PORT));
    
    // Create client socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(client_fd >= 0);
    
    // Connect to server
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_TRUE(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    
    // Accept connection
    ASSERT_OK(ppdb_net_server_accept(server, &conn));
    ASSERT_NOT_NULL(conn);
    
    // Set short timeout
    ASSERT_OK(ppdb_net_set_connection_timeout(conn, 100));
    
    // Wait for timeout
    ppdb_base_sleep(200);
    
    // Check timeout
    ASSERT_OK(check_connection_timeout(conn));
    
    ppdb_connection_state_t state;
    ASSERT_OK(ppdb_net_get_connection_state(conn, &state));
    ASSERT_EQ(state, PPDB_CONN_STATE_CLOSING);
    
    // Cleanup
    close(client_fd);
    ASSERT_OK(cleanup_connection(conn));
    ASSERT_OK(ppdb_net_server_destroy(server));
    return 0;
}

// Client thread function
static void client_thread_func(void* arg) {
    int thread_id = *(int*)arg;
    int connections_per_thread = NUM_CLIENTS / NUM_THREADS;
    int* client_fds = malloc(sizeof(int) * connections_per_thread);
    
    for (int i = 0; i < connections_per_thread; i++) {
        // Create client socket
        client_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_TRUE(client_fds[i] >= 0);
        
        // Connect to server
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TEST_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ASSERT_TRUE(connect(client_fds[i], (struct sockaddr*)&addr, sizeof(addr)) == 0);
        
        atomic_fetch_add(&active_connections, 1);
        
        // Send data
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Data from client %d-%d", thread_id, i);
        ASSERT_TRUE(write(client_fds[i], buffer, strlen(buffer)) > 0);
    }
    
    // Wait a bit
    ppdb_base_sleep(100);
    
    // Close connections
    for (int i = 0; i < connections_per_thread; i++) {
        close(client_fds[i]);
        atomic_fetch_sub(&active_connections, 1);
    }
    
    free(client_fds);
}

// Test concurrent connections
static int test_connection_concurrent(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_base_thread_t* threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    // Create server
    ASSERT_OK(ppdb_net_server_create(&server, TEST_PORT));
    
    // Reset counter
    atomic_store(&active_connections, 0);
    
    // Create client threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        ASSERT_OK(ppdb_base_thread_create(&threads[i], client_thread_func, &thread_ids[i]));
    }
    
    // Handle connections
    while (atomic_load(&active_connections) > 0) {
        ppdb_connection_t* conn = NULL;
        ppdb_error_t err = ppdb_net_server_accept(server, &conn);
        if (err == PPDB_OK && conn) {
            handle_connection_event(conn);
        }
        ppdb_base_sleep(1);
    }
    
    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i]));
        ASSERT_OK(ppdb_base_thread_destroy(threads[i]));
    }
    
    // Cleanup
    ASSERT_OK(ppdb_net_server_destroy(server));
    return 0;
}

// Performance test
static int test_connection_performance(void) {
    ppdb_net_server_t* server = NULL;
    uint64_t start_time, end_time;
    
    // Create server
    ASSERT_OK(ppdb_net_server_create(&server, TEST_PORT));
    
    // Create connections
    ASSERT_OK(ppdb_base_time_get_microseconds(&start_time));
    for (int i = 0; i < NUM_CLIENTS; i++) {
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_TRUE(client_fd >= 0);
        
        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(TEST_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ASSERT_TRUE(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
        
        ppdb_connection_t* conn = NULL;
        ASSERT_OK(ppdb_net_server_accept(server, &conn));
        
        close(client_fd);
        cleanup_connection(conn);
    }
    ASSERT_OK(ppdb_base_time_get_microseconds(&end_time));
    
    printf("Connection creation time: %lu us/conn\n", (end_time - start_time) / NUM_CLIENTS);
    
    // Cleanup
    ASSERT_OK(ppdb_net_server_destroy(server));
    return 0;
}

int main(void) {
    TEST_RUN(test_net_server_basic);
    TEST_RUN(test_net_connection_basic);
    TEST_RUN(test_net_protocol_basic);
    TEST_RUN(test_net_errors);
    TEST_RUN(test_net_concurrent);
    printf("Testing basic connection operations...\n");
    TEST_RUN(test_connection_basic);
    printf("PASSED\n");
    
    printf("Testing connection timeout...\n");
    TEST_RUN(test_connection_timeout);
    printf("PASSED\n");
    
    printf("Testing concurrent connections...\n");
    TEST_RUN(test_connection_concurrent);
    printf("PASSED\n");
    
    printf("Testing connection performance...\n");
    TEST_RUN(test_connection_performance);
    printf("PASSED\n");
    
    return 0;
} 