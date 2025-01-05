/*
 * base.c - PPDB Base Infrastructure Layer Implementation
 *
 * This file is the main entry point for PPDB's base infrastructure layer,
 * responsible for organizing and initializing all base modules.
 * Including:
 * 1. Memory Management (base_memory.inc.c)
 * 2. Synchronization Primitives (base_sync.inc.c)
 * 3. Data Structures (base_struct.inc.c)
 * 4. Utility Functions (base_utils.inc.c)
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Base infrastructure layer structure definition
struct ppdb_base_s {
    // Memory management
    ppdb_base_mempool_t* global_pool;
    ppdb_base_mutex_t* mem_mutex;

    // Synchronization primitives
    ppdb_base_sync_config_t sync_config;
    ppdb_base_mutex_t* global_mutex;

    // Statistics
    struct {
        _Atomic(uint64_t) total_allocs;
        _Atomic(uint64_t) total_frees;
        _Atomic(uint64_t) current_memory;
        _Atomic(uint64_t) peak_memory;
    } stats;
};

// Include module implementations
#include "base/base_error.inc.c"
#include "base/base_memory.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_struct.inc.c"
#include "base/base_utils.inc.c"

// Base infrastructure layer initialization
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config) {
    ppdb_base_t* new_base;
    ppdb_error_t err;

    if (!base || !config) return PPDB_ERR_PARAM;

    // Allocate base structure
    new_base = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_t));
    if (!new_base) return PPDB_ERR_MEMORY;

    memset(new_base, 0, sizeof(ppdb_base_t));

    // Initialize memory management
    err = ppdb_base_memory_init(new_base);
    if (err != PPDB_OK) goto error;

    // Initialize synchronization primitives
    err = ppdb_base_sync_init(new_base);
    if (err != PPDB_OK) goto error;

    // Initialize utility functions
    err = ppdb_base_utils_init(new_base);
    if (err != PPDB_OK) goto error;

    *base = new_base;
    return PPDB_OK;

error:
    ppdb_base_destroy(new_base);
    return err;
}

// Base infrastructure layer cleanup
void ppdb_base_destroy(ppdb_base_t* base) {
    if (!base) return;

    // Clean up in reverse order of dependencies
    ppdb_base_utils_cleanup(base);
    ppdb_base_sync_cleanup(base);
    ppdb_base_memory_cleanup(base);

    ppdb_base_aligned_free(base);
}

// Get statistics
void ppdb_base_get_stats(ppdb_base_t* base, ppdb_base_stats_t* stats) {
    if (!base || !stats) return;

    stats->total_allocs = atomic_load(&base->stats.total_allocs);
    stats->total_frees = atomic_load(&base->stats.total_frees);
    stats->current_memory = atomic_load(&base->stats.current_memory);
    stats->peak_memory = atomic_load(&base->stats.peak_memory);
}
