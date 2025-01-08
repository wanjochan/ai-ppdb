/*
 * base.c - Base Layer Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Error type definition
typedef int ppdb_error_t;

// Include implementations
#include "base/base_error.inc.c"
#include "base/base_memory.inc.c"
#include "base/base_skiplist.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_timer.inc.c"
#include "base/base_utils.inc.c"
#include "base/base_async.inc.c"
#include "base/base_spinlock.inc.c"
#include "base/base_counter.inc.c"
#include "base/base_io.inc.c"

// Global state
static _Atomic(bool) base_initialized = false;

// Base layer initialization
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config) {
    bool expected = false;
    if (!atomic_compare_exchange_strong(&base_initialized, &expected, true)) {
        return PPDB_BASE_ERR_SYSTEM;  // Already initialized
    }

    if (!base || !config) {
        atomic_store(&base_initialized, false);
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_t* new_base = (ppdb_base_t*)malloc(sizeof(ppdb_base_t));
    if (!new_base) {
        atomic_store(&base_initialized, false);
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_base, 0, sizeof(ppdb_base_t));
    memcpy(&new_base->config, config, sizeof(ppdb_base_config_t));
    new_base->initialized = true;

    *base = new_base;
    return PPDB_OK;
}

// Base layer cleanup
void ppdb_base_destroy(ppdb_base_t* base) {
    if (!base) {
        return;
    }

    base->initialized = false;
    free(base);
    atomic_store(&base_initialized, false);
}

// Thread cleanup
void ppdb_base_thread_destroy(ppdb_base_thread_t* thread) {
    if (!thread) {
        return;
    }

    thread->initialized = false;
    free(thread);
}

// Mutex statistics
void ppdb_base_mutex_enable_stats(ppdb_base_mutex_t* mutex, bool enable) {
    // Currently not implemented
    (void)mutex;
    (void)enable;
}
