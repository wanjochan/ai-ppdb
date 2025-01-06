#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "test_common.h"

//-----------------------------------------------------------------------------
// Test Callbacks
//-----------------------------------------------------------------------------

static void on_peer_connection(ppdb_peer_t peer, ppdb_error_t error, void* user_data) {
    bool* connected = user_data;
    *connected = (error == PPDB_OK);
}

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

static void test_peer_server_start_stop(void) {
    // Create context
    ppdb_ctx_t ctx;
    ppdb_options_t options = {
        .db_path = "test_data",
        .cache_size = 1024 * 1024,
        .max_readers = 10,
        .sync_writes = true,
        .flush_period_ms = 1000
    };

    TEST_ASSERT(ppdb_create(&ctx, &options) == PPDB_OK);

    // Configure peer as server
    ppdb_peer_config_t config = {
        .mode = PPDB_PEER_SERVER,
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 10,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Start peer server
    ppdb_peer_t peer;
    TEST_ASSERT(ppdb_peer_create(ctx, &config, &peer) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_start(peer) == PPDB_OK);

    // Get stats
    char stats[1024];
    TEST_ASSERT(ppdb_peer_get_stats(peer, stats, sizeof(stats)) == PPDB_OK);
    TEST_ASSERT(strlen(stats) > 0);

    // Stop peer server
    TEST_ASSERT(ppdb_peer_stop(peer) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_destroy(peer) == PPDB_OK);

    // Cleanup
    TEST_ASSERT(ppdb_destroy(ctx) == PPDB_OK);
}

static void test_peer_connection_callback(void) {
    // Create context
    ppdb_ctx_t ctx;
    ppdb_options_t options = {
        .db_path = "test_data",
        .cache_size = 1024 * 1024,
        .max_readers = 10,
        .sync_writes = true,
        .flush_period_ms = 1000
    };

    TEST_ASSERT(ppdb_create(&ctx, &options) == PPDB_OK);

    // Configure peer server
    ppdb_peer_config_t server_config = {
        .mode = PPDB_PEER_SERVER,
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 10,
        .io_threads = 1,
        .use_tcp_nodelay = true
    };

    // Set connection callback
    bool connected = false;
    server_config.on_connection = on_peer_connection;
    server_config.user_data = &connected;

    // Start peer server
    ppdb_peer_t server;
    TEST_ASSERT(ppdb_peer_create(ctx, &server_config, &server) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_start(server) == PPDB_OK);

    // Connect client peer
    ppdb_peer_config_t client_config = server_config;
    client_config.mode = PPDB_PEER_CLIENT;
    
    ppdb_peer_t client;
    TEST_ASSERT(ppdb_peer_create(ctx, &client_config, &client) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_connect(client) == PPDB_OK);

    // Wait for connection
    usleep(100000);  // 100ms
    TEST_ASSERT(connected);

    // Disconnect peers
    TEST_ASSERT(ppdb_peer_disconnect(client) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_destroy(client) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_stop(server) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_destroy(server) == PPDB_OK);

    // Cleanup
    TEST_ASSERT(ppdb_destroy(ctx) == PPDB_OK);
}

//-----------------------------------------------------------------------------
// Test Runner
//-----------------------------------------------------------------------------

int main(void) {
    TEST_INIT();

    TEST_RUN(test_peer_server_start_stop);
    TEST_RUN(test_peer_connection_callback);

    TEST_CLEANUP();
    return 0;
}