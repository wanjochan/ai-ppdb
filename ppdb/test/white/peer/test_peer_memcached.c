#include "internal/peer.h"
#include "internal/storage.h"

// Test cases
void test_peer_memcached_basic(void) {
    // Create memcached protocol instance
    void* proto;
    const peer_ops_t* ops = peer_get_memcached();
    ppdb_error_t err = ops->create(&proto, NULL);
    assert(err == PPDB_OK);
    assert(proto != NULL);
    
    // Test protocol name
    assert(strcmp(ops->get_name(proto), "memcached") == 0);
    
    // Test connection handling
    ppdb_handle_t handle;
    err = ppdb_conn_create(&handle, ops, NULL);
    assert(err == PPDB_OK);
    assert(handle != 0);
    
    err = ops->on_connect(proto, handle);
    assert(err == PPDB_OK);
    
    // Test data handling
    const char* cmd = "get key\r\n";
    err = ops->on_data(proto, handle, (const uint8_t*)cmd, strlen(cmd));
    assert(err == PPDB_OK);
    
    // Test disconnect
    ops->on_disconnect(proto, handle);
    ppdb_conn_destroy(handle);
    
    // Cleanup
    ops->destroy(proto);
}

int main(void) {
    test_peer_memcached_basic();
    return 0;
} 