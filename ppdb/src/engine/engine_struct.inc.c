/*
 * engine_struct.inc.c - Engine layer data structures implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Forward declarations
static void ppdb_engine_txn_stats_cleanup(ppdb_engine_txn_stats_t* stats);

// Initialize transaction statistics
static ppdb_error_t ppdb_engine_txn_stats_init(ppdb_engine_txn_stats_t* stats) {
    if (!stats) return PPDB_ENGINE_ERR_PARAM;

    // Initialize counters to NULL
    stats->reads = NULL;
    stats->writes = NULL;

    // Create read counter
    ppdb_error_t err = ppdb_base_counter_create(&stats->reads);
    if (err != PPDB_OK) return err;

    // Create write counter
    err = ppdb_base_counter_create(&stats->writes);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(stats->reads);
        stats->reads = NULL;
        return err;
    }

    // Initialize counter values
    ppdb_base_counter_set(stats->reads, 0);
    ppdb_base_counter_set(stats->writes, 0);

    // Initialize state flags
    stats->is_active = false;
    stats->is_committed = false;
    stats->is_rolledback = false;
    stats->error_state = PPDB_OK;

    return PPDB_OK;
}

// Cleanup transaction statistics
static void ppdb_engine_txn_stats_cleanup(ppdb_engine_txn_stats_t* stats) {
    if (!stats) return;

    // Cleanup counters
    if (stats->reads) {
        ppdb_base_counter_destroy(stats->reads);
        stats->reads = NULL;
    }
    if (stats->writes) {
        ppdb_base_counter_destroy(stats->writes);
        stats->writes = NULL;
    }

    // Reset state flags
    stats->is_active = false;
    stats->is_committed = false;
    stats->is_rolledback = false;
    stats->error_state = PPDB_OK;
}

// ... existing code ...