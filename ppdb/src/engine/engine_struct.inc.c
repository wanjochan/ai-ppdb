/*
 * engine_struct.inc.c - Engine layer data structures implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Transaction structure
struct ppdb_engine_txn_s {
    ppdb_engine_t* engine;           // Parent engine
    uint64_t txn_id;                 // Transaction ID
    ppdb_engine_txn_t* next;         // Next transaction in list
    ppdb_engine_txn_stats_t stats;   // Transaction statistics
    bool is_active;                  // Transaction state
};

// Initialize transaction statistics
static ppdb_error_t ppdb_engine_txn_stats_init(ppdb_engine_txn_stats_t* stats) {
    ppdb_error_t err;

    // Create counters
    err = ppdb_base_counter_create(&stats->reads);
    if (err != PPDB_OK) return err;

    err = ppdb_base_counter_create(&stats->writes);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(stats->reads);
        return err;
    }

    stats->is_active = false;
    stats->is_committed = false;
    stats->is_rolledback = false;

    return PPDB_OK;
}

// Cleanup transaction statistics
static void ppdb_engine_txn_stats_cleanup(ppdb_engine_txn_stats_t* stats) {
    if (!stats) return;

    if (stats->reads) {
        ppdb_base_counter_destroy(stats->reads);
    }
    if (stats->writes) {
        ppdb_base_counter_destroy(stats->writes);
    }
}

// Initialize engine statistics
ppdb_error_t ppdb_engine_stats_init(ppdb_engine_stats_t* stats) {
    ppdb_error_t err;

    // Create counters
    err = ppdb_base_counter_create(&stats->total_txns);
    if (err != PPDB_OK) return err;

    err = ppdb_base_counter_create(&stats->active_txns);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(stats->total_txns);
        return err;
    }

    err = ppdb_base_counter_create(&stats->total_reads);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(stats->total_txns);
        ppdb_base_counter_destroy(stats->active_txns);
        return err;
    }

    err = ppdb_base_counter_create(&stats->total_writes);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(stats->total_txns);
        ppdb_base_counter_destroy(stats->active_txns);
        ppdb_base_counter_destroy(stats->total_reads);
        return err;
    }

    return PPDB_OK;
}

// Cleanup engine statistics
void ppdb_engine_stats_cleanup(ppdb_engine_stats_t* stats) {
    if (!stats) return;

    if (stats->total_txns) {
        ppdb_base_counter_destroy(stats->total_txns);
    }
    if (stats->active_txns) {
        ppdb_base_counter_destroy(stats->active_txns);
    }
    if (stats->total_reads) {
        ppdb_base_counter_destroy(stats->total_reads);
    }
    if (stats->total_writes) {
        ppdb_base_counter_destroy(stats->total_writes);
    }
} 