/*
 * database_txn.inc.c - Transaction Management Implementation
 */

// Transaction ID counter
static _Atomic(uint64_t) next_txn_id = 1;

// Transaction begin
ppdb_error_t ppdb_txn_begin(ppdb_database_t* db, ppdb_txn_t** txn, uint32_t flags) {
    if (!db || !db->initialized || !txn) return PPDB_DATABASE_ERR_START;

    ppdb_txn_t* new_txn = ppdb_base_malloc(sizeof(ppdb_txn_t));
    if (!new_txn) return PPDB_BASE_ERR_MEMORY;

    new_txn->db = db;
    new_txn->flags = flags;
    new_txn->isolation = db->config.default_isolation;
    new_txn->txn_id = atomic_fetch_add(&next_txn_id, 1);
    new_txn->active = true;

    // Update statistics
    ppdb_database_stats_t delta = {0};
    delta.total_txns = 1;
    database_update_stats(db, &delta);

    *txn = new_txn;
    return PPDB_OK;
}

// Transaction commit
ppdb_error_t ppdb_txn_commit(ppdb_txn_t* txn) {
    if (!txn || !txn->active) return PPDB_DATABASE_ERR_TXN;

    ppdb_error_t err = PPDB_OK;
    ppdb_database_t* db = txn->db;

    // Handle MVCC if enabled
    if (db->config.enable_mvcc) {
        err = ppdb_mvcc_commit(db->mvcc, txn);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // Sync to disk if required
    if ((txn->flags & PPDB_TXN_SYNC) || db->config.sync_on_commit) {
        err = ppdb_storage_sync(db->storage);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // Update statistics
    ppdb_database_stats_t delta = {0};
    delta.committed_txns = 1;
    database_update_stats(db, &delta);

    txn->active = false;
    ppdb_base_free(txn);
    return PPDB_OK;
}

// Transaction abort
ppdb_error_t ppdb_txn_abort(ppdb_txn_t* txn) {
    if (!txn || !txn->active) return PPDB_DATABASE_ERR_TXN;

    ppdb_error_t err = PPDB_OK;
    ppdb_database_t* db = txn->db;

    // Handle MVCC if enabled
    if (db->config.enable_mvcc) {
        err = ppdb_mvcc_abort(db->mvcc, txn);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // Update statistics
    ppdb_database_stats_t delta = {0};
    delta.aborted_txns = 1;
    database_update_stats(db, &delta);

    txn->active = false;
    ppdb_base_free(txn);
    return PPDB_OK;
}

// Get transaction isolation level
ppdb_error_t ppdb_txn_get_isolation(ppdb_txn_t* txn, ppdb_txn_isolation_t* isolation) {
    if (!txn || !isolation) return PPDB_DATABASE_ERR_TXN;
    *isolation = txn->isolation;
    return PPDB_OK;
}

// Set transaction isolation level
ppdb_error_t ppdb_txn_set_isolation(ppdb_txn_t* txn, ppdb_txn_isolation_t isolation) {
    if (!txn || !txn->active) return PPDB_DATABASE_ERR_TXN;
    if (isolation < PPDB_TXN_READ_UNCOMMITTED || isolation > PPDB_TXN_SERIALIZABLE) {
        return PPDB_DATABASE_ERR_TXN;
    }
    txn->isolation = isolation;
    return PPDB_OK;
} 