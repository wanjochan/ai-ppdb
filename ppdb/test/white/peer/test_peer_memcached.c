#include "../../../src/peer.h"
#include "../../../src/internal/peer.h"
#include "../test.h"

// Mock memcached commands
static const char *SET_CMD = "set key 0 0 5\r\nvalue\r\n";
static const char *GET_CMD = "get key\r\n";
static const char *DELETE_CMD = "delete key\r\n";

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

    // Connect to memcached server
    TEST_ASSERT(ppdb_peer_connect(&peer, "localhost", 11211) == PPDB_OK);

    // Test SET command
    TEST_ASSERT(ppdb_peer_send(&peer, SET_CMD, strlen(SET_CMD)) == PPDB_OK);
    
    char response[128] = {0};
    size_t received = 0;
    TEST_ASSERT(ppdb_peer_recv(&peer, response, sizeof(response), &received) == PPDB_OK);
    
    // Test GET command
    TEST_ASSERT(ppdb_peer_send(&peer, GET_CMD, strlen(GET_CMD)) == PPDB_OK);
    
    memset(response, 0, sizeof(response));
    TEST_ASSERT(ppdb_peer_recv(&peer, response, sizeof(response), &received) == PPDB_OK);
    
    // Test DELETE command
    TEST_ASSERT(ppdb_peer_send(&peer, DELETE_CMD, strlen(DELETE_CMD)) == PPDB_OK);
    
    memset(response, 0, sizeof(response));
    TEST_ASSERT(ppdb_peer_recv(&peer, response, sizeof(response), &received) == PPDB_OK);

    // Disconnect and cleanup
    TEST_ASSERT(ppdb_peer_disconnect(&peer) == PPDB_OK);
    TEST_ASSERT(ppdb_peer_cleanup(&peer) == PPDB_OK);

    return 0;
} 