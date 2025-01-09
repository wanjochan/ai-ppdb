#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_macros.h"
#include "test_net.h"

#define PERF_PORT 12346
#define PERF_HOST "127.0.0.1"
#define PERF_BUFFER_SIZE 4096
#define NUM_CONCURRENT_CLIENTS 100
#define NUM_MESSAGES_PER_CLIENT 1000
#define MESSAGE_SIZE 1024
#define TEST_DURATION_SEC 60

typedef struct {
    int thread_id;
    const char* host;
    uint16_t port;
    uint64_t* latencies;
    size_t latency_count;
    uint64_t total_bytes;
    uint64_t total_messages;
} perf_client_context_t;

static void perf_client_thread_func(void* arg);
static void perf_server_handler(ppdb_connection_t* conn, uint32_t events);

// 吞吐量测试
int test_net_throughput(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_base_thread_t* clients[NUM_CONCURRENT_CLIENTS];
    perf_client_context_t* contexts[NUM_CONCURRENT_CLIENTS];
    uint64_t start_time, end_time;
    uint64_t total_bytes = 0;
    uint64_t total_messages = 0;

    // 创建并启动服务器
    ASSERT_OK(ppdb_base_net_server_create(&server));
    ASSERT_OK(ppdb_base_net_server_start(server));

    // 分配上下文
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        contexts[i] = malloc(sizeof(perf_client_context_t));
        contexts[i]->thread_id = i;
        contexts[i]->host = PERF_HOST;
        contexts[i]->port = PERF_PORT;
        contexts[i]->total_bytes = 0;
        contexts[i]->total_messages = 0;
    }

    // 记录开始时间
    start_time = ppdb_base_get_time_ns();

    // 启动客户端线程
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        ASSERT_OK(ppdb_base_thread_create(&clients[i], perf_client_thread_func, contexts[i]));
    }

    // 等待测试时间
    ppdb_base_sleep(TEST_DURATION_SEC * 1000);

    // 等待所有客户端完成
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        ASSERT_OK(ppdb_base_thread_join(clients[i]));
        ASSERT_OK(ppdb_base_thread_destroy(clients[i]));
        
        total_bytes += contexts[i]->total_bytes;
        total_messages += contexts[i]->total_messages;
        free(contexts[i]);
    }

    // 记录结束时间
    end_time = ppdb_base_get_time_ns();

    // 计算统计信息
    double duration = (end_time - start_time) / 1e9;  // 转换为秒
    double throughput_mbps = (total_bytes * 8.0) / (duration * 1024 * 1024);  // Mbps
    double messages_per_sec = total_messages / duration;

    printf("Network Performance Test Results:\n");
    printf("Duration: %.2f seconds\n", duration);
    printf("Total Data: %lu bytes\n", total_bytes);
    printf("Total Messages: %lu\n", total_messages);
    printf("Throughput: %.2f Mbps\n", throughput_mbps);
    printf("Message Rate: %.2f msg/s\n", messages_per_sec);

    // 清理
    ASSERT_OK(ppdb_base_net_server_stop(server));
    ASSERT_OK(ppdb_base_net_server_destroy(server));

    return 0;
}

// 延迟测试
int test_net_latency(void) {
    ppdb_net_server_t* server = NULL;
    ppdb_base_thread_t* clients[NUM_CONCURRENT_CLIENTS];
    perf_client_context_t* contexts[NUM_CONCURRENT_CLIENTS];
    uint64_t* all_latencies = NULL;
    size_t total_samples = 0;

    // 创建并启动服务器
    ASSERT_OK(ppdb_base_net_server_create(&server));
    ASSERT_OK(ppdb_base_net_server_start(server));

    // 分配延迟数组
    size_t latencies_per_client = NUM_MESSAGES_PER_CLIENT;
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        contexts[i] = malloc(sizeof(perf_client_context_t));
        contexts[i]->thread_id = i;
        contexts[i]->host = PERF_HOST;
        contexts[i]->port = PERF_PORT;
        contexts[i]->latencies = malloc(sizeof(uint64_t) * latencies_per_client);
        contexts[i]->latency_count = 0;
    }

    // 启动客户端线程
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        ASSERT_OK(ppdb_base_thread_create(&clients[i], perf_client_thread_func, contexts[i]));
    }

    // 等待所有客户端完成
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        ASSERT_OK(ppdb_base_thread_join(clients[i]));
        ASSERT_OK(ppdb_base_thread_destroy(clients[i]));
        total_samples += contexts[i]->latency_count;
    }

    // 合并所有延迟数据
    all_latencies = malloc(sizeof(uint64_t) * total_samples);
    size_t current_index = 0;
    for (int i = 0; i < NUM_CONCURRENT_CLIENTS; i++) {
        memcpy(all_latencies + current_index, 
               contexts[i]->latencies,
               contexts[i]->latency_count * sizeof(uint64_t));
        current_index += contexts[i]->latency_count;
        free(contexts[i]->latencies);
        free(contexts[i]);
    }

    // 计算统计信息
    qsort(all_latencies, total_samples, sizeof(uint64_t), compare_uint64);
    uint64_t min_latency = all_latencies[0];
    uint64_t max_latency = all_latencies[total_samples - 1];
    uint64_t median_latency = all_latencies[total_samples / 2];
    uint64_t p95_latency = all_latencies[(size_t)(total_samples * 0.95)];
    uint64_t p99_latency = all_latencies[(size_t)(total_samples * 0.99)];

    printf("Latency Test Results:\n");
    printf("Total Samples: %zu\n", total_samples);
    printf("Min Latency: %lu ns\n", min_latency);
    printf("Max Latency: %lu ns\n", max_latency);
    printf("Median Latency: %lu ns\n", median_latency);
    printf("95th Percentile: %lu ns\n", p95_latency);
    printf("99th Percentile: %lu ns\n", p99_latency);

    // 清理
    free(all_latencies);
    ASSERT_OK(ppdb_base_net_server_stop(server));
    ASSERT_OK(ppdb_base_net_server_destroy(server));

    return 0;
}

// 连接处理能力测试
int test_net_connection_capacity(void) {
    ppdb_net_server_t* server = NULL;
    int* client_fds = NULL;
    int num_connections = 0;
    const int max_test_connections = 10000;

    // 创建并启动服务器
    ASSERT_OK(ppdb_base_net_server_create(&server));
    ASSERT_OK(ppdb_base_net_server_start(server));

    // 分配文件描述符数组
    client_fds = malloc(sizeof(int) * max_test_connections);

    // 尝试建立尽可能多的连接
    for (int i = 0; i < max_test_connections; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) break;

        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PERF_PORT),
            .sin_addr.s_addr = inet_addr(PERF_HOST)
        };

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            client_fds[num_connections++] = fd;
        } else {
            close(fd);
            break;
        }

        if (i % 100 == 0) {
            printf("Established %d connections\n", i + 1);
        }
    }

    printf("Connection Capacity Test Results:\n");
    printf("Maximum Concurrent Connections: %d\n", num_connections);

    // 关闭所有连接
    for (int i = 0; i < num_connections; i++) {
        close(client_fds[i]);
    }

    // 清理
    free(client_fds);
    ASSERT_OK(ppdb_base_net_server_stop(server));
    ASSERT_OK(ppdb_base_net_server_destroy(server));

    return 0;
}

static void perf_client_thread_func(void* arg) {
    perf_client_context_t* ctx = (perf_client_context_t*)arg;
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    char* buffer = malloc(MESSAGE_SIZE);
    uint64_t start_time, end_time;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ctx->port),
        .sin_addr.s_addr = inet_addr(ctx->host)
    };

    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        // 填充测试数据
        memset(buffer, 'A', MESSAGE_SIZE - 1);
        buffer[MESSAGE_SIZE - 1] = '\0';

        for (int i = 0; i < NUM_MESSAGES_PER_CLIENT; i++) {
            start_time = ppdb_base_get_time_ns();
            
            // 发送数据
            if (send(client_fd, buffer, MESSAGE_SIZE, 0) > 0) {
                ctx->total_bytes += MESSAGE_SIZE;
                ctx->total_messages++;
            }

            // 接收响应
            if (recv(client_fd, buffer, MESSAGE_SIZE, 0) > 0) {
                end_time = ppdb_base_get_time_ns();
                if (ctx->latencies && ctx->latency_count < NUM_MESSAGES_PER_CLIENT) {
                    ctx->latencies[ctx->latency_count++] = end_time - start_time;
                }
            }
        }
    }

    free(buffer);
    close(client_fd);
}

static void perf_server_handler(ppdb_connection_t* conn, uint32_t events) {
    char buffer[PERF_BUFFER_SIZE];
    ssize_t bytes;

    if (events & PPDB_EVENT_READ) {
        bytes = recv(conn->fd, buffer, PERF_BUFFER_SIZE, 0);
        if (bytes > 0) {
            send(conn->fd, buffer, bytes, 0);
        }
    }
}

static int compare_uint64(const void* a, const void* b) {
    uint64_t va = *(const uint64_t*)a;
    uint64_t vb = *(const uint64_t*)b;
    return (va > vb) - (va < vb);
}

int main(void) {
    TEST_RUN(test_net_throughput);
    TEST_RUN(test_net_latency);
    TEST_RUN(test_net_connection_capacity);
    return 0;
} 