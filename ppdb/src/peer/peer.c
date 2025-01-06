#include <cosmopolitan.h>
#include "internal/peer.h"
#include "internal/peer_internal.h"

// Include protocol adapter implementations
#include "peer_memcached.inc.c"
#include "peer_redis.inc.c"

// Peer context structure
struct peer_ctx {
    int initialized;
    // ... other fields to be added ...
};

// Global peer context
static struct peer_ctx g_peer_ctx;

// Initialize peer subsystem
int peer_init(void) {
    memset(&g_peer_ctx, 0, sizeof(g_peer_ctx));
    g_peer_ctx.initialized = 1;
    return 0;
}

// Cleanup peer subsystem
void peer_cleanup(void) {
    if (!g_peer_ctx.initialized) {
        return;
    }
    // ... cleanup code ...
    g_peer_ctx.initialized = 0;
}

// Check if peer subsystem is initialized
int peer_is_initialized(void) {
    return g_peer_ctx.initialized;
}

// Get memcached protocol adapter
const peer_ops_t* peer_get_memcached(void) {
    static const peer_ops_t ops = {
        .create = memcached_proto_create,
        .destroy = memcached_proto_destroy,
        .on_connect = memcached_proto_on_connect,
        .on_disconnect = memcached_proto_on_disconnect,
        .on_data = memcached_proto_on_data,
        .get_name = memcached_proto_get_name
    };
    return &ops;
}

// Get redis protocol adapter
const peer_ops_t* peer_get_redis(void) {
    static const peer_ops_t ops = {
        .create = redis_proto_create,
        .destroy = redis_proto_destroy,
        .on_connect = redis_proto_on_connect,
        .on_disconnect = redis_proto_on_disconnect,
        .on_data = redis_proto_on_data,
        .get_name = redis_proto_get_name
    };
    return &ops;
}
