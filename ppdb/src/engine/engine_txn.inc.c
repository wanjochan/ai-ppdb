/*
 * engine_txn.inc.c - Engine transaction management implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Transaction management initialization
ppdb_error_t ppdb_engine_txn_init(ppdb_engine_t* engine) {
    ppdb_error_t err;

    if (!engine) return PPDB_ENGINE_ERR_PARAM;

    // Initialize transaction mutex
    err = ppdb_base_mutex_create(&engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) return err;

    // Initialize transaction list
    engine->txn_mgr.next_txn_id = 1;
    engine->txn_mgr.active_txns = NULL;

    return PPDB_OK;
}

// Transaction management cleanup
void ppdb_engine_txn_cleanup(ppdb_engine_t* engine) {
    if (!engine) return;

    // Cleanup active transactions
    ppdb_engine_txn_t* txn = engine->txn_mgr.active_txns;
    while (txn) {
        ppdb_engine_txn_t* next = txn->next;
        ppdb_engine_txn_stats_cleanup(&txn->stats);
        ppdb_base_aligned_free(txn);
        txn = next;
    }

    // Cleanup transaction mutex
    if (engine->txn_mgr.txn_mutex) {
        ppdb_base_mutex_destroy(engine->txn_mgr.txn_mutex);
        engine->txn_mgr.txn_mutex = NULL;
    }
}

// Begin a new transaction
ppdb_error_t ppdb_engine_txn_begin(ppdb_engine_t* engine, ppdb_engine_txn_t** txn) {
    ppdb_error_t err;
    ppdb_engine_txn_t* new_txn;

    if (!engine || !txn) return PPDB_ENGINE_ERR_PARAM;

    // Create new transaction
    new_txn = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_engine_txn_t));
    if (!new_txn) return PPDB_ERR_MEMORY;

    // Initialize transaction
    memset(new_txn, 0, sizeof(ppdb_engine_txn_t));
    new_txn->engine = engine;

    // Initialize transaction statistics
    err = ppdb_engine_txn_stats_init(&new_txn->stats);
    if (err != PPDB_OK) {
        ppdb_base_aligned_free(new_txn);
        return err;
    }

    // Set transaction state
    new_txn->stats.is_active = true;
    new_txn->stats.is_committed = false;
    new_txn->stats.is_rolledback = false;

    // Lock transaction manager
    err = ppdb_base_mutex_lock(engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) {
        ppdb_engine_txn_stats_cleanup(&new_txn->stats);
        ppdb_base_aligned_free(new_txn);
        return err;
    }

    // Assign transaction ID and add to list
    new_txn->txn_id = engine->txn_mgr.next_txn_id++;
    new_txn->next = engine->txn_mgr.active_txns;
    engine->txn_mgr.active_txns = new_txn;

    // Update statistics
    ppdb_base_counter_increment(engine->stats.total_txns);
    ppdb_base_counter_increment(engine->stats.active_txns);

    // Unlock transaction manager
    ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);

    *txn = new_txn;
    return PPDB_OK;
}

// Commit a transaction
ppdb_error_t ppdb_engine_txn_commit(ppdb_engine_txn_t* txn) {
    ppdb_error_t err;
    ppdb_engine_t* engine;

    if (!txn || !txn->engine) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    engine = txn->engine;

    // Lock transaction manager
    err = ppdb_base_mutex_lock(engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) return err;

    // Remove from active transactions list
    ppdb_engine_txn_t** curr = &engine->txn_mgr.active_txns;
    while (*curr && *curr != txn) {
        curr = &(*curr)->next;
    }
    if (*curr) {
        *curr = txn->next;
    }

    // Update statistics
    ppdb_base_counter_decrement(engine->stats.active_txns);
    txn->stats.is_active = false;
    txn->stats.is_committed = true;

    // Unlock transaction manager
    ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);

    return PPDB_OK;
}

// Rollback a transaction
ppdb_error_t ppdb_engine_txn_rollback(ppdb_engine_txn_t* txn) {
    ppdb_error_t err;
    ppdb_engine_t* engine;

    if (!txn || !txn->engine) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    engine = txn->engine;

    // Lock transaction manager
    err = ppdb_base_mutex_lock(engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) return err;

    // Remove from active transactions list
    ppdb_engine_txn_t** curr = &engine->txn_mgr.active_txns;
    while (*curr && *curr != txn) {
        curr = &(*curr)->next;
    }
    if (*curr) {
        *curr = txn->next;
    }

    // Update statistics
    ppdb_base_counter_decrement(engine->stats.active_txns);
    txn->stats.is_active = false;
    txn->stats.is_rolledback = true;

    // Unlock transaction manager
    ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);

    return PPDB_OK;
}

// Get transaction statistics
void ppdb_engine_txn_get_stats(ppdb_engine_txn_t* txn, ppdb_engine_txn_stats_t* stats) {
    if (!txn || !stats) return;

    // Copy statistics
    stats->reads = txn->stats.reads;
    stats->writes = txn->stats.writes;
    stats->is_active = txn->stats.is_active;
    stats->is_committed = txn->stats.is_committed;
    stats->is_rolledback = txn->stats.is_rolledback;
}