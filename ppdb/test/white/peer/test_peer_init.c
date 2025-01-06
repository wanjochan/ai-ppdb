#include <ppdb/ppdb.h>
#include "../../../src/internal/peer.h"
#include "../test_framework.h"

int main(void) {
    ppdb_peer_t peer;
    ppdb_peer_config_t config = {
        .max_connections = 10,
        .connect_timeout_ms = 1000,
        .read_timeout_ms = 2000,
        .write_timeout_ms = 2000
    };

    // Test invalid parameters
    TEST_ASSERT(ppdb_peer_init(NULL, &config) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_init(&peer, NULL) == PPDB_ERR_INVALID_PARAM);

    // Test successful initialization
    TEST_ASSERT(ppdb_peer_init(&peer, &config) == PPDB_OK);
    TEST_ASSERT(peer.config.max_connections == config.max_connections);
    TEST_ASSERT(peer.config.connect_timeout_ms == config.connect_timeout_ms);
    TEST_ASSERT(peer.config.read_timeout_ms == config.read_timeout_ms);
    TEST_ASSERT(peer.config.write_timeout_ms == config.write_timeout_ms);
    TEST_ASSERT(peer.conn_pool == NULL);
    TEST_ASSERT(peer.proto_handlers == NULL);

    // Test cleanup
    TEST_ASSERT(ppdb_peer_cleanup(NULL) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_cleanup(&peer) == PPDB_OK);

    return 0;
} 