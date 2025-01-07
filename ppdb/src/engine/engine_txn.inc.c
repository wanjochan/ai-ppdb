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
    if (*txn) return PPDB_ENGINE_ERR_PARAM;

    // Create transaction structure
    ppdb_engine_txn_t* new_txn = malloc(sizeof(ppdb_engine_txn_t));
    if (!new_txn) return PPDB_ENGINE_ERR_INIT;

    // Initialize transaction structure
    memset(new_txn, 0, sizeof(ppdb_engine_txn_t));
    new_txn->engine = engine;

    // Initialize transaction statistics
    new_txn->stats.reads = NULL;
    new_txn->stats.writes = NULL;
    new_txn->stats.is_active = false;
    new_txn->stats.is_committed = false;
    new_txn->stats.is_rolledback = false;

    ppdb_error_t err = ppdb_base_counter_create(&new_txn->stats.reads);
    if (err != PPDB_OK) {
        free(new_txn);
        return err;
    }

    err = ppdb_base_counter_create(&new_txn->stats.writes);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(new_txn->stats.reads);
        free(new_txn);
        return err;
    }

    // Lock transaction manager
    err = ppdb_base_mutex_lock(engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(new_txn->stats.writes);
        ppdb_base_counter_destroy(new_txn->stats.reads);
        free(new_txn);
        return err;
    }

    // Assign transaction ID
    new_txn->id = engine->txn_mgr.next_txn_id++;

    // Add to active transactions list
    new_txn->next = engine->txn_mgr.active_txns;
    engine->txn_mgr.active_txns = new_txn;

    // Update statistics
    ppdb_base_counter_inc(engine->stats.total_txns);
    ppdb_base_counter_inc(engine->stats.active_txns);

    // Unlock transaction manager
    err = ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) {
        ppdb_base_counter_dec(engine->stats.active_txns);
        ppdb_base_counter_dec(engine->stats.total_txns);
        ppdb_base_counter_destroy(new_txn->stats.writes);
        ppdb_base_counter_destroy(new_txn->stats.reads);
        free(new_txn);
        return err;
    }

    new_txn->stats.is_active = true;
    *txn = new_txn;
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_txn_commit(ppdb_engine_txn_t* txn) {
    if (!txn) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    ppdb_engine_t* engine = txn->engine;

    // Lock transaction manager
    ppdb_error_t err = ppdb_base_mutex_lock(engine->txn_mgr.txn_mutex);
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
    ppdb_base_counter_dec(engine->stats.active_txns);

    // Unlock transaction manager
    ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);

    // Update transaction state
    txn->stats.is_active = false;
    txn->stats.is_committed = true;

    // Cleanup transaction
    ppdb_base_counter_destroy(txn->stats.reads);
    ppdb_base_counter_destroy(txn->stats.writes);
    free(txn);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_txn_rollback(ppdb_engine_txn_t* txn) {
    if (!txn) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    ppdb_engine_t* engine = txn->engine;

    // Lock transaction manager
    ppdb_error_t err = ppdb_base_mutex_lock(engine->txn_mgr.txn_mutex);
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
    ppdb_base_counter_dec(engine->stats.active_txns);

    // Unlock transaction manager
    ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);

    // Update transaction state
    txn->stats.is_active = false;
    txn->stats.is_rolledback = true;

    // Cleanup transaction
    ppdb_base_counter_destroy(txn->stats.reads);
    ppdb_base_counter_destroy(txn->stats.writes);
    free(txn);

    return PPDB_OK;
}