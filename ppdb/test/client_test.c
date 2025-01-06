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

static void on_peer_operation(ppdb_error_t error, void* result, void* user_data) {
    ppdb_data_t* value = result;
    ppdb_data_t* expected = user_data;

    if (expected) {
        TEST_ASSERT(error == PPDB_OK);
        TEST_ASSERT(value->size == expected->size);
        if (value->size <= sizeof(value->inline_data)) {
            TEST_ASSERT(memcmp(value->inline_data, expected->inline_data, value->size) == 0);
        } else {
            TEST_ASSERT(memcmp(value->extended_data, expected->extended_data, value->size) == 0);
        }
    } else {
        TEST_ASSERT(error == PPDB_ERR_NOT_FOUND);
    }
}

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

static void test_peer_connect_disconnect(void) {
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

    // Configure peer
    ppdb_peer_config_t config = {
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 1,
        .io_threads = 1,
        .use_tcp_nodelay = true,
        .role = PPDB_PEER_CLIENT
    };

    // Connect peer
    ppdb_peer_t peer;
    TEST_ASSERT(ppdb_peer_connect(ctx, &config, &peer) == PPDB_OK);

    // Disconnect peer
    TEST_ASSERT(ppdb_peer_disconnect(peer) == PPDB_OK);

    // Cleanup
    TEST_ASSERT(ppdb_destroy(ctx) == PPDB_OK);
}

static void test_peer_operations(void) {
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

    // Configure peer
    ppdb_peer_config_t config = {
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 1000,
        .max_connections = 1,
        .io_threads = 1,
        .use_tcp_nodelay = true,
        .role = PPDB_PEER_CLIENT
    };

    // Connect peer
    ppdb_peer_t peer;
    TEST_ASSERT(ppdb_peer_connect(ctx, &config, &peer) == PPDB_OK);

    // Test data
    ppdb_data_t key = {
        .inline_data = "test_key",
        .size = 8,
        .flags = 0
    };

    ppdb_data_t value = {
        .inline_data = "test_value",
        .size = 10,
        .flags = 0
    };

    // Put value
    TEST_ASSERT(ppdb_peer_put(peer, &key, &value, on_peer_operation, &value) == PPDB_OK);

    // Get value
    TEST_ASSERT(ppdb_peer_get(peer, &key, on_peer_operation, &value) == PPDB_OK);

    // Delete value
    TEST_ASSERT(ppdb_peer_delete(peer, &key, on_peer_operation, NULL) == PPDB_OK);

    // Get non-existent value
    TEST_ASSERT(ppdb_peer_get(peer, &key, on_peer_operation, NULL) == PPDB_OK);

    // Disconnect peer
    TEST_ASSERT(ppdb_peer_disconnect(peer) == PPDB_OK);

    // Cleanup
    TEST_ASSERT(ppdb_destroy(ctx) == PPDB_OK);
}

//-----------------------------------------------------------------------------
// Test Runner
//-----------------------------------------------------------------------------

int main(void) {
    TEST_INIT();

    TEST_RUN(test_peer_connect_disconnect);
    TEST_RUN(test_peer_operations);

    TEST_CLEANUP();
    return 0;
}