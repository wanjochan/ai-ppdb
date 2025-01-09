/*
 * base.c - Base Layer Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Error type definition
typedef int ppdb_error_t;

// Include implementations
#include "base/base_core.inc.c"
#include "base/base_memory.inc.c"
#include "base/base_utils.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_async.inc.c"

// Global state
static _Atomic(bool) base_initialized = false;

// Base layer initialization
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config) {
    if (!base || !config) return PPDB_BASE_ERR_PARAM;
    if (base_initialized) return PPDB_BASE_ERR_EXISTS;

    ppdb_base_t* new_base = malloc(sizeof(ppdb_base_t));
    if (!new_base) return PPDB_BASE_ERR_MEMORY;

    memcpy(&new_base->config, config, sizeof(ppdb_base_config_t));
    new_base->initialized = true;
    new_base->lock = NULL;
    new_base->mempool = NULL;
    new_base->async_loop = NULL;
    new_base->io_manager = NULL;

    *base = new_base;
    base_initialized = true;
    return PPDB_OK;
}

// Base layer cleanup
void ppdb_base_destroy(ppdb_base_t* base) {
    if (!base) return;

    if (base->io_manager) {
        ppdb_base_io_manager_destroy(base->io_manager);
    }

    if (base->async_loop) {
        // TODO: Cleanup async loop
    }

    if (base->mempool) {
        ppdb_base_mempool_destroy(base->mempool);
    }

    if (base->lock) {
        ppdb_base_mutex_destroy(base->lock);
    }

    base->initialized = false;
    free(base);
    base_initialized = false;
}

// Mutex statistics
void ppdb_base_mutex_enable_stats(ppdb_base_mutex_t* mutex, bool enable) {
    // Currently not implemented
    (void)mutex;
    (void)enable;
}
