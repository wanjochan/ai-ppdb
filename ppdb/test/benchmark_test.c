#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "test_common.h"

//-----------------------------------------------------------------------------
// Test Data
//-----------------------------------------------------------------------------

#define TEST_PORT 11211
#define TEST_THREADS 4
#define TEST_CONNECTIONS 10
#define TEST_ITERATIONS 100000

typedef struct {
    ppdb_ctx_t ctx;
    ppdb_conn_t conn;
    int id;
    int success;
    int failure;
    int64_t total_time_us;  // Total operation time in microseconds
} client_context_t;

//-----------------------------------------------------------------------------
// Test Callbacks
//-----------------------------------------------------------------------------

static void on_operation_complete(ppdb_error_t error, void* result, void* user_data) {
    client_context_t* ctx = user_data;
    if (error == PPDB_OK) {
        ctx->success++;
    } else {
        ctx->failure++;
    }
}

//-----------------------------------------------------------------------------
// Benchmark Cases
//-----------------------------------------------------------------------------

static void benchmark_single_connection(void) {
    // Create server context
    ppdb_ctx_t server_ctx;
    ppdb_options_t server_options = {
        .db_path = "test_data",
        .cache_size = 1024 * 1024 * 1024,
        .max_readers = 1,
        .sync_writes = false,
        .flush_period_ms = 1000
    };

    TEST_ASSERT(ppdb_create(&server_ctx, &server_options) == PPDB_OK);

    // Configure server
    ppdb_net_config_t server_config = {
        .host = "127.0.0.1",
        .port = TEST_PORT,
        .timeout_ms = 1000,
        .max_connections = 1,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Start server
    TEST_ASSERT(ppdb_server_start(server_ctx, &server_config) == PPDB_OK);

    // Create client
    client_context_t client = {0};
    ppdb_options_t client_options = {
        .db_path = NULL,
        .cache_size = 0,
        .max_readers = 1,
        .sync_writes = false,
        .flush_period_ms = 0
    };

    TEST_ASSERT(ppdb_create(&client.ctx, &client_options) == PPDB_OK);

    // Configure client
    ppdb_net_config_t client_config = {
        .host = "127.0.0.1",
        .port = TEST_PORT,
        .timeout_ms = 1000,
        .max_connections = 1,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Connect client
    TEST_ASSERT(ppdb_client_connect(client.ctx, &client_config, &client.conn) == PPDB_OK);

    // Run benchmark
    int64_t start_time = ppdb_base_get_time_us();

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // Prepare data
        char key_buf[32];
        char value_buf[32];
        snprintf(key_buf, sizeof(key_buf), "key_%d", i);
        snprintf(value_buf, sizeof(value_buf), "value_%d", i);

        ppdb_data_t key = {
            .size = strlen(key_buf),
            .flags = 0
        };
        memcpy(key.inline_data, key_buf, key.size);

        ppdb_data_t value = {
            .size = strlen(value_buf),
            .flags = 0
        };
        memcpy(value.inline_data, value_buf, value.size);

        // Put value
        TEST_ASSERT(ppdb_client_put(client.conn, &key, &value,
                                  on_operation_complete, &client) == PPDB_OK);

        // Get value
        TEST_ASSERT(ppdb_client_get(client.conn, &key,
                                  on_operation_complete, &client) == PPDB_OK);

        // Delete value
        TEST_ASSERT(ppdb_client_delete(client.conn, &key,
                                     on_operation_complete, &client) == PPDB_OK);
    }

    int64_t end_time = ppdb_base_get_time_us();
    client.total_time_us = end_time - start_time;

    // Print results
    printf("Single Connection Benchmark:\n");
    printf("  Operations: %d\n", TEST_ITERATIONS * 3);
    printf("  Success: %d\n", client.success);
    printf("  Failure: %d\n", client.failure);
    printf("  Total Time: %.2f seconds\n", client.total_time_us / 1000000.0);
    printf("  Ops/Second: %.2f\n", 
           (TEST_ITERATIONS * 3.0 * 1000000.0) / client.total_time_us);

    // Cleanup
    TEST_ASSERT(ppdb_client_disconnect(client.conn) == PPDB_OK);
    TEST_ASSERT(ppdb_destroy(client.ctx) == PPDB_OK);
    TEST_ASSERT(ppdb_server_stop(server_ctx) == PPDB_OK);
    TEST_ASSERT(ppdb_destroy(server_ctx) == PPDB_OK);
}

static void benchmark_concurrent_connections(void) {
    // Create server context
    ppdb_ctx_t server_ctx;
    ppdb_options_t server_options = {
        .db_path = "test_data",
        .cache_size = 1024 * 1024 * 1024,
        .max_readers = TEST_CONNECTIONS,
        .sync_writes = false,
        .flush_period_ms = 1000
    };

    TEST_ASSERT(ppdb_create(&server_ctx, &server_options) == PPDB_OK);

    // Configure server
    ppdb_net_config_t server_config = {
        .host = "127.0.0.1",
        .port = TEST_PORT,
        .timeout_ms = 1000,
        .max_connections = TEST_CONNECTIONS,
        .io_threads = TEST_THREADS,
        .use_tcp_nodelay = true
    };

    // Start server
    TEST_ASSERT(ppdb_server_start(server_ctx, &server_config) == PPDB_OK);

    // Create clients
    client_context_t clients[TEST_CONNECTIONS] = {0};

    for (int i = 0; i < TEST_CONNECTIONS; i++) {
        // Create client context
        ppdb_options_t client_options = {
            .db_path = NULL,
            .cache_size = 0,
            .max_readers = 1,
            .sync_writes = false,
            .flush_period_ms = 0
        };

        TEST_ASSERT(ppdb_create(&clients[i].ctx, &client_options) == PPDB_OK);

        // Configure client
        ppdb_net_config_t client_config = {
            .host = "127.0.0.1",
            .port = TEST_PORT,
            .timeout_ms = 1000,
            .max_connections = 1,
            .io_threads = 1,
            .use_tcp_nodelay = true
        };

        // Connect client
        TEST_ASSERT(ppdb_client_connect(clients[i].ctx, &client_config,
                                      &clients[i].conn) == PPDB_OK);

        clients[i].id = i;
    }

    // Run benchmark
    int64_t start_time = ppdb_base_get_time_us();

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        for (int j = 0; j < TEST_CONNECTIONS; j++) {
            // Prepare data
            char key_buf[32];
            char value_buf[32];
            snprintf(key_buf, sizeof(key_buf), "key_%d_%d", j, i);
            snprintf(value_buf, sizeof(value_buf), "value_%d_%d", j, i);

            ppdb_data_t key = {
                .size = strlen(key_buf),
                .flags = 0
            };
            memcpy(key.inline_data, key_buf, key.size);

            ppdb_data_t value = {
                .size = strlen(value_buf),
                .flags = 0
            };
            memcpy(value.inline_data, value_buf, value.size);

            // Put value
            TEST_ASSERT(ppdb_client_put(clients[j].conn, &key, &value,
                                      on_operation_complete, &clients[j]) == PPDB_OK);

            // Get value
            TEST_ASSERT(ppdb_client_get(clients[j].conn, &key,
                                      on_operation_complete, &clients[j]) == PPDB_OK);

            // Delete value
            TEST_ASSERT(ppdb_client_delete(clients[j].conn, &key,
                                         on_operation_complete, &clients[j]) == PPDB_OK);
        }
    }

    int64_t end_time = ppdb_base_get_time_us();
    int64_t total_time_us = end_time - start_time;

    // Print results
    printf("\nConcurrent Connections Benchmark:\n");
    printf("  Connections: %d\n", TEST_CONNECTIONS);
    printf("  Operations per Connection: %d\n", TEST_ITERATIONS * 3);
    printf("  Total Operations: %d\n", TEST_ITERATIONS * 3 * TEST_CONNECTIONS);
    printf("  Total Time: %.2f seconds\n", total_time_us / 1000000.0);
    printf("  Total Ops/Second: %.2f\n",
           (TEST_ITERATIONS * 3.0 * TEST_CONNECTIONS * 1000000.0) / total_time_us);

    for (int i = 0; i < TEST_CONNECTIONS; i++) {
        printf("  Client %d: %d success, %d failure\n",
               i, clients[i].success, clients[i].failure);
    }

    // Cleanup
    for (int i = 0; i < TEST_CONNECTIONS; i++) {
        TEST_ASSERT(ppdb_client_disconnect(clients[i].conn) == PPDB_OK);
        TEST_ASSERT(ppdb_destroy(clients[i].ctx) == PPDB_OK);
    }

    TEST_ASSERT(ppdb_server_stop(server_ctx) == PPDB_OK);
    TEST_ASSERT(ppdb_destroy(server_ctx) == PPDB_OK);
}

//-----------------------------------------------------------------------------
// Test Runner
//-----------------------------------------------------------------------------

int main(void) {
    TEST_INIT();

    TEST_RUN(benchmark_single_connection);
    TEST_RUN(benchmark_concurrent_connections);

    TEST_CLEANUP();
    return 0;
} 