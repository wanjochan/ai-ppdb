#include "internal/peer.h"
#include "internal/storage.h"

// Test cases
void test_peer_conn_basic(void) {
    ppdb_handle_t conn;
    const peer_ops_t* ops = peer_get_memcached();
    
    // Create connection
    ppdb_error_t err = ppdb_conn_create(&conn, ops, NULL);
    assert(err == PPDB_OK);
    assert(conn != NULL);
    
    // Check initial state
    assert(!ppdb_conn_is_connected(conn));
    assert(strcmp(ppdb_conn_get_proto_name(conn), "memcached") == 0);
    
    // Set socket
    err = ppdb_conn_set_socket(conn, 1);
    assert(err == PPDB_OK);
    assert(ppdb_conn_is_connected(conn));
    
    // Close connection
    ppdb_conn_close(conn);
    assert(!ppdb_conn_is_connected(conn));
    
    // Cleanup
    ppdb_conn_destroy(conn);
}

int main(void) {
    test_peer_conn_basic();
    return 0;
} 