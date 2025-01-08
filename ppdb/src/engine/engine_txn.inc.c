/*
 * engine_txn.inc.c - Engine Transaction Management Implementation
 */

// Transaction initialization
ppdb_error_t ppdb_engine_txn_init(ppdb_engine_t* engine) {
    if (!engine) return PPDB_ENGINE_ERR_PARAM;
    if (!engine->base) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Initialize transaction manager
    memset(&engine->txn_mgr, 0, sizeof(ppdb_engine_txn_mgr_t));

    // Initialize transaction mutex
    ppdb_error_t err = ppdb_base_mutex_create(&engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) {
        printf("ERROR: Failed to create transaction mutex (code: %d)\n", err);
        return err;
    }

    // Initialize transaction ID and list
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
        if (txn->stats.is_active) {
            ppdb_engine_txn_rollback(txn);
        }
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
    if (!engine->base) return PPDB_ENGINE_ERR_INVALID_STATE;
    if (*txn) return PPDB_ENGINE_ERR_PARAM;  // Don't allow overwriting existing transaction

    // Lock transaction manager
    ppdb_error_t err = ppdb_base_mutex_lock(engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) return err;

    // Allocate transaction structure
    ppdb_engine_txn_t* new_txn = malloc(sizeof(ppdb_engine_txn_t));
    if (!new_txn) {
        ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);
        return PPDB_ENGINE_ERR_MEMORY;
    }

    // Initialize transaction
    memset(new_txn, 0, sizeof(ppdb_engine_txn_t));  // Zero out all fields
    new_txn->engine = engine;
    new_txn->id = engine->txn_mgr.next_txn_id++;
    new_txn->next = NULL;

    // Initialize transaction statistics
    err = ppdb_engine_txn_stats_init(&new_txn->stats);
    if (err != PPDB_OK) {
        free(new_txn);
        ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);
        return err;
    }

    // Initialize transaction counters
    err = ppdb_base_counter_create(&new_txn->stats.reads);
    if (err != PPDB_OK) {
        ppdb_engine_txn_stats_cleanup(&new_txn->stats);
        free(new_txn);
        ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);
        return err;
    }

    err = ppdb_base_counter_create(&new_txn->stats.writes);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(new_txn->stats.reads);
        ppdb_engine_txn_stats_cleanup(&new_txn->stats);
        free(new_txn);
        ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);
        return err;
    }

    // Set initial counter values
    ppdb_base_counter_set(new_txn->stats.reads, 0);
    ppdb_base_counter_set(new_txn->stats.writes, 0);

    // Set transaction state
    new_txn->stats.is_active = true;
    new_txn->stats.is_committed = false;
    new_txn->stats.is_rolledback = false;
    new_txn->stats.error_state = PPDB_OK;

    // Update engine statistics
    ppdb_base_counter_inc(engine->stats.total_txns);
    ppdb_base_counter_inc(engine->stats.active_txns);

    // Add to active transactions list
    new_txn->next = engine->txn_mgr.active_txns;
    engine->txn_mgr.active_txns = new_txn;

    // Unlock transaction manager
    ppdb_base_mutex_unlock(engine->txn_mgr.txn_mutex);

    *txn = new_txn;
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_txn_commit(ppdb_engine_txn_t* txn) {
    if (!txn) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->engine || !txn->engine->base) return PPDB_ENGINE_ERR_INVALID_STATE;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock transaction manager
    ppdb_error_t err = ppdb_base_mutex_lock(txn->engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) return err;

    // Update engine statistics before state change
    ppdb_base_counter_add(txn->engine->stats.total_reads, ppdb_base_counter_get(txn->stats.reads));
    ppdb_base_counter_add(txn->engine->stats.total_writes, ppdb_base_counter_get(txn->stats.writes));
    ppdb_base_counter_dec(txn->engine->stats.active_txns);

    // Remove from active transactions list
    ppdb_engine_txn_t** curr = &txn->engine->txn_mgr.active_txns;
    while (*curr && *curr != txn) {
        curr = &(*curr)->next;
    }
    if (*curr) {
        *curr = txn->next;
    }

    // Mark transaction as committed
    txn->stats.is_active = false;
    txn->stats.is_committed = true;
    txn->stats.is_rolledback = false;
    txn->stats.error_state = PPDB_OK;

    // Unlock transaction manager
    ppdb_base_mutex_unlock(txn->engine->txn_mgr.txn_mutex);

    // Cleanup transaction counters
    if (txn->stats.reads) {
        ppdb_base_counter_destroy(txn->stats.reads);
        txn->stats.reads = NULL;
    }
    if (txn->stats.writes) {
        ppdb_base_counter_destroy(txn->stats.writes);
        txn->stats.writes = NULL;
    }

    // Free transaction structure
    free(txn);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_txn_rollback(ppdb_engine_txn_t* txn) {
    if (!txn) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->engine || !txn->engine->base) return PPDB_ENGINE_ERR_INVALID_STATE;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock transaction manager
    ppdb_error_t err = ppdb_base_mutex_lock(txn->engine->txn_mgr.txn_mutex);
    if (err != PPDB_OK) return err;

    // Update engine statistics before state change
    ppdb_base_counter_dec(txn->engine->stats.active_txns);

    // Remove from active transactions list
    ppdb_engine_txn_t** curr = &txn->engine->txn_mgr.active_txns;
    while (*curr && *curr != txn) {
        curr = &(*curr)->next;
    }
    if (*curr) {
        *curr = txn->next;
    }

    // Mark transaction as rolled back
    txn->stats.is_active = false;
    txn->stats.is_committed = false;
    txn->stats.is_rolledback = true;
    txn->stats.error_state = PPDB_ENGINE_ERR_TXN;

    // Unlock transaction manager
    ppdb_base_mutex_unlock(txn->engine->txn_mgr.txn_mutex);

    // Cleanup transaction counters
    if (txn->stats.reads) {
        ppdb_base_counter_destroy(txn->stats.reads);
        txn->stats.reads = NULL;
    }
    if (txn->stats.writes) {
        ppdb_base_counter_destroy(txn->stats.writes);
        txn->stats.writes = NULL;
    }

    // Free transaction structure
    free(txn);

    return PPDB_OK;
}

void ppdb_engine_txn_get_stats(ppdb_engine_txn_t* txn, ppdb_engine_txn_stats_t* stats) {
    if (!txn || !stats) return;
    if (!txn->engine || !txn->engine->base) return;

    // Copy transaction statistics
    memcpy(stats, &txn->stats, sizeof(ppdb_engine_txn_stats_t));
}