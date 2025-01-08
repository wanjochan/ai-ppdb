/*
 * engine_stats.inc.c - Engine Statistics Implementation
 */

// Statistics initialization
ppdb_error_t ppdb_engine_stats_init(ppdb_engine_stats_t* stats) {
    if (!stats) return PPDB_ENGINE_ERR_PARAM;

    // Create counters
    ppdb_error_t err = ppdb_base_counter_create(&stats->total_txns);
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

void ppdb_engine_stats_cleanup(ppdb_engine_stats_t* stats) {
    if (!stats) return;

    // Destroy counters
    if (stats->total_txns) ppdb_base_counter_destroy(stats->total_txns);
    if (stats->active_txns) ppdb_base_counter_destroy(stats->active_txns);
    if (stats->total_reads) ppdb_base_counter_destroy(stats->total_reads);
    if (stats->total_writes) ppdb_base_counter_destroy(stats->total_writes);
}

void ppdb_engine_get_stats(ppdb_engine_t* engine, ppdb_engine_stats_t* stats) {
    if (!engine || !stats) return;

    // Copy statistics
    *stats = engine->stats;
} 