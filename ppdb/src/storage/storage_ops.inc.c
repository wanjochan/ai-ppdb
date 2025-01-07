//-----------------------------------------------------------------------------
// Storage Operations Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_storage_put(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             const void* value, size_t value_size) {
    if (!table || !key || !value) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0 || value_size == 0) return PPDB_STORAGE_ERR_PARAM;

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(table->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Put data using engine
    err = ppdb_engine_put(tx, table->engine_table, key, key_size, value, value_size);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    // Update table size
    atomic_fetch_add(&table->size, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             void* value, size_t* value_size) {
    if (!table || !key || !value || !value_size) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0) return PPDB_STORAGE_ERR_PARAM;

    // Begin read transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(table->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Get data using engine
    err = ppdb_engine_get(tx, table->engine_table, key, key_size, value, value_size);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return PPDB_STORAGE_ERR_NOT_FOUND;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_delete(ppdb_storage_table_t* table, const void* key, size_t key_size) {
    if (!table || !key) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0) return PPDB_STORAGE_ERR_PARAM;

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(table->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Delete data using engine
    err = ppdb_engine_delete(tx, table->engine_table, key, key_size);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return PPDB_STORAGE_ERR_NOT_FOUND;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    // Update table size
    atomic_fetch_sub(&table->size, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan(ppdb_storage_table_t* table, ppdb_storage_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_STORAGE_ERR_PARAM;

    // Begin read transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(table->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Initialize cursor
    cursor->table = table;
    cursor->valid = false;

    // Open cursor in engine
    err = ppdb_engine_cursor_open(tx, table->engine_table, &cursor->engine_cursor);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    // Move cursor to first entry
    err = ppdb_engine_cursor_first(cursor->engine_cursor);
    if (err != PPDB_OK) {
        ppdb_engine_cursor_close(cursor->engine_cursor);
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    cursor->valid = true;
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan_next(ppdb_storage_table_t* table, ppdb_storage_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_STORAGE_ERR_PARAM;
    if (!cursor->valid) return PPDB_STORAGE_ERR_INVALID_STATE;

    // Move cursor to next entry
    ppdb_error_t err = ppdb_engine_cursor_next(cursor->engine_cursor);
    if (err != PPDB_OK) {
        cursor->valid = false;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_compact(ppdb_storage_table_t* table) {
    if (!table) return PPDB_STORAGE_ERR_PARAM;

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(table->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Compact table using engine
    err = ppdb_engine_compact(tx, table->engine_table);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_storage_table_t* table) {
    if (!table) return PPDB_STORAGE_ERR_PARAM;

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(table->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Flush table using engine
    err = ppdb_engine_flush(tx, table->engine_table);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    return PPDB_OK;
}
