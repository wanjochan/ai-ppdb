#include <cosmopolitan.h>
#include "internal/peer.h"
#include "internal/storage.h"
#include "peer/peer_memcached.inc.c"
#include "peer/peer_redis.inc.c"
#include "peer/peer_conn.inc.c"

// Global state
static bool g_initialized = false;

// Initialize peer layer
int peer_init(void) {
    if (g_initialized) {
        return PPDB_OK;
    }
    g_initialized = true;
    return PPDB_OK;
}

// Cleanup peer layer
void peer_cleanup(void) {
    if (!g_initialized) {
        return;
    }
    g_initialized = false;
}

// Check if peer layer is initialized
int peer_is_initialized(void) {
    return g_initialized;
} 