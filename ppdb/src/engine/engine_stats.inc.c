/*
 * engine_stats.inc.c - Engine statistics implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Initialize engine statistics
ppdb_error_t ppdb_engine_stats_init(ppdb_engine_stats_t* stats) {
    if (!stats) return PPDB_ENGINE_ERR_PARAM;

    // Create total transactions counter
    ppdb_error_t err = ppdb_base_counter_create(&stats->total_txns, "total_txns");
    if (err != PPDB_OK) return err;

    // Create active transactions counter
    err = ppdb_base_counter_create(&stats->active_txns, "active_txns");
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(stats->total_txns);
        return err;
    }

    // Create total reads counter
    err = ppdb_base_counter_create(&stats->total_reads, "total_reads");
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(stats->total_txns);
        ppdb_base_counter_destroy(stats->active_txns);
        return err;
    }

    // Create total writes counter
    err = ppdb_base_counter_create(&stats->total_writes, "total_writes");
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

    // Destroy counters
    if (stats->total_txns) {
        ppdb_base_counter_destroy(stats->total_txns);
        stats->total_txns = NULL;
    }
    if (stats->active_txns) {
        ppdb_base_counter_destroy(stats->active_txns);
        stats->active_txns = NULL;
    }
    if (stats->total_reads) {
        ppdb_base_counter_destroy(stats->total_reads);
        stats->total_reads = NULL;
    }
    if (stats->total_writes) {
        ppdb_base_counter_destroy(stats->total_writes);
        stats->total_writes = NULL;
    }
}

// Get engine statistics
void ppdb_engine_get_stats(ppdb_engine_t* engine, ppdb_engine_stats_t* stats) {
    if (!engine || !stats) return;

    // Copy counter values
    stats->total_txns = engine->stats.total_txns;
    stats->active_txns = engine->stats.active_txns;
    stats->total_reads = engine->stats.total_reads;
    stats->total_writes = engine->stats.total_writes;
} 