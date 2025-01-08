#include <cosmopolitan.h>
#include "internal/peer.h"
#include "internal/database.h"

// Initialize peer layer
ppdb_error_t ppdb_peer_init(void) {
    return PPDB_OK;
}

// Cleanup peer layer
void ppdb_peer_cleanup(void) {
}

// Create peer instance
ppdb_error_t ppdb_peer_create(ppdb_ctx_t* ctx, ppdb_peer_t** peer) {
    if (!ctx || !peer) {
        return PPDB_ERR_PARAM;
    }

    *peer = calloc(1, sizeof(ppdb_peer_t));
    if (!*peer) {
        return PPDB_ERR_MEMORY;
    }

    (*peer)->ctx = ctx;
    return PPDB_OK;
}

// Destroy peer instance
void ppdb_peer_destroy(ppdb_peer_t* peer) {
    if (!peer) {
        return;
    }

    free(peer);
}

// Check if peer layer is initialized
int peer_is_initialized(void) {
    return g_initialized;
} 