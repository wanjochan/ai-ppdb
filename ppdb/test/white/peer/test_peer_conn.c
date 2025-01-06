#include "../../../src/peer.h"
#include "../../../src/internal/peer.h"
#include "../test.h"

int main(void) {
    ppdb_peer_t peer;
    ppdb_peer_config_t config = {
        .max_connections = 10,
        .connect_timeout_ms = 1000,
        .read_timeout_ms = 2000,
        .write_timeout_ms = 2000
    };

    // Initialize peer
    TEST_ASSERT(ppdb_peer_init(&peer, &config) == PPDB_OK);

    // Test invalid parameters
    TEST_ASSERT(ppdb_peer_connect(NULL, "localhost", 8080) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_connect(&peer, NULL, 8080) == PPDB_ERR_INVALID_PARAM);

    // Test successful connection
    TEST_ASSERT(ppdb_peer_connect(&peer, "localhost", 8080) == PPDB_OK);

    // Test disconnect
    TEST_ASSERT(ppdb_peer_disconnect(NULL) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_disconnect(&peer) == PPDB_OK);

    // Cleanup
    TEST_ASSERT(ppdb_peer_cleanup(&peer) == PPDB_OK);

    return 0;
} 