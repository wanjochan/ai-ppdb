/*
 * base.c - Base Layer Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Error type definition
typedef int ppdb_error_t;

// Include implementation files
#include "base/base_error.inc.c"
#include "base/base_sync.inc.c"
#include "base/base_memory.inc.c"
#include "base/base_utils.inc.c"
#include "base/base_counter.inc.c"
#include "base/base_timer.inc.c"
#include "base/base_skiplist.inc.c"
#include "base/base_async.inc.c"
#include "base/base_io.inc.c"

// Global state
static bool base_initialized = false;

// Base layer initialization
ppdb_error_t ppdb_base_init(void) {
    if (base_initialized) {
        return PPDB_OK;
    }

    ppdb_error_t err;

    // Initialize error handling
    err = ppdb_error_init();
    if (err != PPDB_OK) {
        return err;
    }

    // Initialize memory management
    err = ppdb_base_memory_init();
    if (err != PPDB_OK) {
        return err;
    }

    base_initialized = true;
    return PPDB_OK;
}

// Base layer cleanup
void ppdb_base_cleanup(void) {
    if (!base_initialized) {
        return;
    }

    // Cleanup in reverse order of initialization
    ppdb_base_memory_cleanup();
    ppdb_error_cleanup();

    base_initialized = false;
}
