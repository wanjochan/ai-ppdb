/*
 * engine_struct.inc.c - Engine structure initialization and cleanup
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Initialize transaction statistics
ppdb_error_t ppdb_engine_txn_stats_init(ppdb_engine_txn_stats_t* stats) {
    if (!stats) return PPDB_ENGINE_ERR_PARAM;

    // Initialize all counters to 0
    stats->read_count = 0;
    stats->write_count = 0;
    stats->delete_count = 0;
    stats->conflict_count = 0;
    stats->rollback_count = 0;
    stats->commit_count = 0;
    stats->duration_ms = 0;

    return PPDB_OK;
}

// Cleanup transaction statistics
void ppdb_engine_txn_stats_cleanup(ppdb_engine_txn_stats_t* stats) {
    if (!stats) return;

    // Reset all counters
    stats->read_count = 0;
    stats->write_count = 0;
    stats->delete_count = 0;
    stats->conflict_count = 0;
    stats->rollback_count = 0;
    stats->commit_count = 0;
    stats->duration_ms = 0;
}

// Initialize transaction manager
ppdb_error_t ppdb_engine_txn_mgr_init(ppdb_engine_txn_mgr_t* mgr) {
    if (!mgr) return PPDB_ENGINE_ERR_PARAM;

    // Create transaction manager mutex
    ppdb_error_t err = ppdb_base_mutex_create(&mgr->lock);
    if (err != PPDB_OK) return err;

    // Initialize transaction ID counter
    mgr->next_txn_id = 1;
    mgr->active_txns = NULL;

    return PPDB_OK;
}

// Cleanup transaction manager
void ppdb_engine_txn_mgr_cleanup(ppdb_engine_txn_mgr_t* mgr) {
    if (!mgr) return;

    // Destroy transaction manager mutex
    if (mgr->lock) {
        ppdb_base_mutex_destroy(mgr->lock);
        mgr->lock = NULL;
    }

    // Reset transaction ID counter
    mgr->next_txn_id = 0;
    mgr->active_txns = NULL;
}

// Initialize IO manager
ppdb_error_t ppdb_engine_io_mgr_init(ppdb_engine_io_mgr_t* mgr) {
    if (!mgr) return PPDB_ENGINE_ERR_PARAM;

    // Create IO manager
    ppdb_error_t err = ppdb_base_io_manager_create(&mgr->io_mgr);
    if (err != PPDB_OK) return err;

    // Initialize IO thread
    mgr->io_thread = NULL;
    mgr->io_running = false;

    return PPDB_OK;
}

// Cleanup IO manager
void ppdb_engine_io_mgr_cleanup(ppdb_engine_io_mgr_t* mgr) {
    if (!mgr) return;

    // Stop IO thread if running
    if (mgr->io_running) {
        mgr->io_running = false;
        if (mgr->io_thread) {
            ppdb_base_thread_join(mgr->io_thread);
            ppdb_base_thread_destroy(mgr->io_thread);
            mgr->io_thread = NULL;
        }
    }

    // Destroy IO manager
    if (mgr->io_mgr) {
        ppdb_base_io_manager_destroy(mgr->io_mgr);
        mgr->io_mgr = NULL;
    }
}

// Initialize table list
ppdb_error_t ppdb_engine_table_list_init(ppdb_engine_table_list_t* list, ppdb_engine_t* engine) {
    if (!list || !engine) return PPDB_ENGINE_ERR_PARAM;

    // Create table list mutex
    ppdb_error_t err = ppdb_base_mutex_create(&list->lock);
    if (err != PPDB_OK) return err;

    // Create skiplist for table entries
    err = ppdb_base_skiplist_create(&list->skiplist, ppdb_engine_compare_table_name);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(list->lock);
        list->lock = NULL;
        return err;
    }

    // Set engine reference
    list->engine = engine;

    return PPDB_OK;
}

// Cleanup table list
void ppdb_engine_table_list_cleanup(ppdb_engine_table_list_t* list) {
    if (!list) return;

    // Destroy skiplist
    if (list->skiplist) {
        ppdb_base_skiplist_destroy(list->skiplist);
        list->skiplist = NULL;
    }

    // Destroy mutex
    if (list->lock) {
        ppdb_base_mutex_destroy(list->lock);
        list->lock = NULL;
    }

    // Clear engine reference
    list->engine = NULL;
}

// ... existing code ...