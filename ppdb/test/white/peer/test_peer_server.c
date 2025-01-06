#include <cosmopolitan.h>
#include "../test_framework.h"
#include "internal/peer.h"
#include "internal/storage.h"

// Test cases
static int test_peer_server_basic(void) {
    ppdb_handle_t db;
    ppdb_error_t err;

    // Create database
    err = ppdb_create(&db, NULL);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(db);

    // Create server
    ppdb_server_t server;
    ppdb_net_config_t config = {
        .host = "127.0.0.1",
        .port = 11211,
        .timeout_ms = 30000,
        .max_connections = 10,
        .io_threads = 4,
        .use_tcp_nodelay = true,
        .protocol = "memcached"
    };

    err = ppdb_server_create(&server, db, &config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(server);

    // Start server
    err = ppdb_server_start(server);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Stop server
    err = ppdb_server_stop(server);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Cleanup
    ppdb_server_destroy(server);
    ppdb_destroy(db);

    return 0;
}

int main(void) {
    TEST_INIT();

    TEST_RUN(test_peer_server_basic);

    TEST_CLEANUP();
    return 0;
} 