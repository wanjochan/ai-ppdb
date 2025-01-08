/*
 * engine_txn.inc.c - Engine Transaction Management Implementation
 */

// Transaction initialization
ppdb_error_t ppdb_engine_txn_init(ppdb_engine_t* engine) {
    if (!engine) return PPDB_ENGINE_ERR_PARAM;

    // Initialize transaction mutex
    ppdb_error_t err = ppdb_base_mutex_create(&engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) return err;

    // Initialize transaction ID
    engine->txn_mgr.next_txn_id = 1;
    engine->txn_mgr.active_txns = NULL;

    return PPDB_OK;
}

void ppdb_engine_txn_cleanup(ppdb_engine_t* engine) {
    if (!engine) return;

    // Rollback all active transactions
    ppdb_engine_txn_t* txn = engine->txn_mgr.active_txns;
    while (txn) {
        ppdb_engine_txn_t* next = txn->next;
        ppdb_engine_txn_rollback(txn);
        txn = next;
    }

    // Destroy transaction mutex
    if (engine->txn_mgr.txn_mutex) {
        ppdb_base_mutex_destroy(engine->txn_mgr.txn_mutex);
        engine->txn_mgr.txn_mutex = NULL;
    }
}

ppdb_error_t ppdb_engine_txn_begin(ppdb_engine_t* engine, ppdb_engine_txn_t** txn) {
    if (!engine || !txn) return PPDB_ENGINE_ERR_PARAM;
    if (*txn) return PPDB_ENGINE_ERR_PARAM;  // Don't allow overwriting existing transaction

    // Allocate transaction structure
    ppdb_engine_txn_t* new_txn = malloc(sizeof(ppdb_engine_txn_t));
    if (!new_txn) return PPDB_ENGINE_ERR_MEMORY;

    // Initialize transaction
    new_txn->engine = engine;
    new_txn->is_write = true;  // For now, all transactions are write transactions
    new_txn->is_active = true;
    
    // Initialize statistics
    ppdb_base_counter_init(&new_txn->stats.reads);
    ppdb_base_counter_init(&new_txn->stats.writes);

    *txn = new_txn;
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_txn_commit(ppdb_engine_txn_t* txn) {
    if (!txn) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock engine
    ppdb_error_t err = ppdb_base_mutex_lock(txn->engine->lock);
    if (err != PPDB_OK) return err;

    // Update engine statistics
    ppdb_base_counter_add(&txn->engine->stats.total_reads, ppdb_base_counter_get(&txn->stats.reads));
    ppdb_base_counter_add(&txn->engine->stats.total_writes, ppdb_base_counter_get(&txn->stats.writes));

    // Unlock engine
    ppdb_base_mutex_unlock(txn->engine->lock);

    // Mark transaction as inactive
    txn->is_active = false;

    // Free transaction
    free(txn);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_txn_rollback(ppdb_engine_txn_t* txn) {
    if (!txn) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Mark transaction as inactive
    txn->is_active = false;

    // Free transaction
    free(txn);

    return PPDB_OK;
}