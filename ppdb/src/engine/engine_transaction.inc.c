#include <cosmopolitan.h>

static uint64_t get_next_txn_id(ppdb_engine_t* engine) {
    ppdb_sync_lock(engine->txn_lock);
    uint64_t id = engine->next_txn_id++;
    ppdb_sync_unlock(engine->txn_lock);
    return id;
}

static uint64_t get_next_ts(ppdb_engine_t* engine) {
    ppdb_sync_lock(engine->txn_lock);
    uint64_t ts = engine->next_ts++;
    ppdb_sync_unlock(engine->txn_lock);
    return ts;
}

ppdb_error_t ppdb_txn_begin(ppdb_engine_t* engine, ppdb_isolation_level_t isolation, ppdb_txn_t** txn) {
    ppdb_txn_t* new_txn = PPDB_ALIGNED_ALLOC(sizeof(ppdb_txn_t));
    if (!new_txn) return PPDB_ERROR_OOM;
    
    // Initialize transaction
    new_txn->txn_id = get_next_txn_id(engine);
    new_txn->status = PPDB_TXN_ACTIVE;
    new_txn->isolation = isolation;
    new_txn->start_ts = get_next_ts(engine);
    new_txn->commit_ts = 0;
    
    // Create transaction lock
    ppdb_error_t err = ppdb_sync_create(&new_txn->lock, 0);  // mutex
    if (err != PPDB_OK) {
        PPDB_ALIGNED_FREE(new_txn);
        return err;
    }
    
    // Add to active transactions list
    ppdb_sync_lock(engine->txn_lock);
    new_txn->next = engine->active_txns;
    engine->active_txns = new_txn;
    ppdb_sync_unlock(engine->txn_lock);
    
    *txn = new_txn;
    return PPDB_OK;
}

ppdb_error_t ppdb_txn_commit(ppdb_engine_t* engine, ppdb_txn_t* txn) {
    if (!txn) return PPDB_ERROR_INVALID;
    
    ppdb_sync_lock(txn->lock);
    
    if (txn->status != PPDB_TXN_ACTIVE) {
        ppdb_sync_unlock(txn->lock);
        return PPDB_ERROR_TXN_STATE;
    }
    
    // Set commit timestamp and status
    txn->commit_ts = get_next_ts(engine);
    txn->status = PPDB_TXN_COMMITTED;
    
    ppdb_sync_unlock(txn->lock);
    
    // Remove from active transactions list
    ppdb_sync_lock(engine->txn_lock);
    ppdb_txn_t** curr = &engine->active_txns;
    while (*curr && *curr != txn) {
        curr = &(*curr)->next;
    }
    if (*curr) {
        *curr = txn->next;
    }
    ppdb_sync_unlock(engine->txn_lock);
    
    return PPDB_OK;
}

ppdb_error_t ppdb_txn_abort(ppdb_engine_t* engine, ppdb_txn_t* txn) {
    if (!txn) return PPDB_ERROR_INVALID;
    
    ppdb_sync_lock(txn->lock);
    
    if (txn->status != PPDB_TXN_ACTIVE) {
        ppdb_sync_unlock(txn->lock);
        return PPDB_ERROR_TXN_STATE;
    }
    
    // Set status to aborted
    txn->status = PPDB_TXN_ABORTED;
    
    ppdb_sync_unlock(txn->lock);
    
    // Remove from active transactions list
    ppdb_sync_lock(engine->txn_lock);
    ppdb_txn_t** curr = &engine->active_txns;
    while (*curr && *curr != txn) {
        curr = &(*curr)->next;
    }
    if (*curr) {
        *curr = txn->next;
    }
    ppdb_sync_unlock(engine->txn_lock);
    
    return PPDB_OK;
}

void ppdb_txn_destroy(ppdb_txn_t* txn) {
    if (!txn) return;
    if (txn->lock) {
        ppdb_sync_destroy(txn->lock);
    }
    PPDB_ALIGNED_FREE(txn);
} 