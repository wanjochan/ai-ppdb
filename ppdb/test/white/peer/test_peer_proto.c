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

    // Test data transfer
    const char *test_data = "Hello, peer!";
    char recv_buffer[64] = {0};
    size_t received = 0;

    // Test send with invalid parameters
    TEST_ASSERT(ppdb_peer_send(NULL, test_data, strlen(test_data)) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_send(&peer, NULL, strlen(test_data)) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_send(&peer, test_data, 0) == PPDB_ERR_INVALID_PARAM);

    // Test receive with invalid parameters
    TEST_ASSERT(ppdb_peer_recv(NULL, recv_buffer, sizeof(recv_buffer), &received) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_recv(&peer, NULL, sizeof(recv_buffer), &received) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_recv(&peer, recv_buffer, 0, &received) == PPDB_ERR_INVALID_PARAM);
    TEST_ASSERT(ppdb_peer_recv(&peer, recv_buffer, sizeof(recv_buffer), NULL) == PPDB_ERR_INVALID_PARAM);

    // Test successful send
    TEST_ASSERT(ppdb_peer_send(&peer, test_data, strlen(test_data)) == PPDB_OK);

    // Test successful receive
    TEST_ASSERT(ppdb_peer_recv(&peer, recv_buffer, sizeof(recv_buffer), &received) == PPDB_OK);

    // Cleanup
    TEST_ASSERT(ppdb_peer_cleanup(&peer) == PPDB_OK);

    return 0;
} 