#include "internal/peer.h"
#include "internal/storage.h"

// Test cases
void test_peer_proto_basic(void) {
    // Test memcached protocol
    const peer_ops_t* memcached = peer_get_memcached();
    assert(memcached != NULL);
    assert(memcached->create != NULL);
    assert(memcached->destroy != NULL);
    assert(memcached->on_connect != NULL);
    assert(memcached->on_disconnect != NULL);
    assert(memcached->on_data != NULL);
    assert(memcached->get_name != NULL);
    
    void* proto;
    ppdb_error_t err = memcached->create(&proto, NULL);
    assert(err == PPDB_OK);
    assert(proto != NULL);
    
    assert(strcmp(memcached->get_name(proto), "memcached") == 0);
    
    memcached->destroy(proto);
    
    // Test redis protocol
    const peer_ops_t* redis = peer_get_redis();
    assert(redis != NULL);
    assert(redis->create != NULL);
    assert(redis->destroy != NULL);
    assert(redis->on_connect != NULL);
    assert(redis->on_disconnect != NULL);
    assert(redis->on_data != NULL);
    assert(redis->get_name != NULL);
    
    err = redis->create(&proto, NULL);
    assert(err == PPDB_OK);
    assert(proto != NULL);
    
    assert(strcmp(redis->get_name(proto), "redis") == 0);
    
    redis->destroy(proto);
}

int main(void) {
    test_peer_proto_basic();
    return 0;
} 