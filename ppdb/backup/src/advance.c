#include "ppdb/ppdb_advance.h"
#include "ppdb/ppdb.h"
#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// Performance Metrics Implementation
//-----------------------------------------------------------------------------

static ppdb_error_t metrics_get_impl(ppdb_base_t* base,
                                   ppdb_metrics_t* metrics) {
    if (!base || !metrics) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Copy metrics atomically
    metrics->get_count = atomic_load(&base->metrics.get_count.value);
    metrics->get_hits = atomic_load(&base->metrics.get_hits.value);
    metrics->put_count = atomic_load(&base->metrics.put_count.value);
    metrics->remove_count = atomic_load(&base->metrics.remove_count.value);

    return PPDB_OK;
}

// Get storage stats
ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_metrics_t* metrics) {
    return metrics_get_impl(base, metrics);
}

// Get storage operations
ppdb_error_t ppdb_storage_get_ops(ppdb_base_t* base, ppdb_advance_ops_t* ops) {
    if (!base || !ops) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Set operations
    ops->metrics_get = metrics_get_impl;

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Advanced Features Initialization
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_advance_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;
    
    // Allocate advanced operations structure
    base->advance = calloc(1, sizeof(ppdb_advance_ops_t));
    if (!base->advance) return PPDB_ERR_OUT_OF_MEMORY;
    
    // Set performance metrics implementation
    base->advance->metrics_get = metrics_get_impl;
    
    return PPDB_OK;
}

void ppdb_advance_cleanup(ppdb_base_t* base) {
    if (base && base->advance) {
        free(base->advance);
        base->advance = NULL;
    }
}
