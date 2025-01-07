//-----------------------------------------------------------------------------
// Storage Operations Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_storage_put(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             const void* value, size_t value_size) {
    if (!table || !key || !value) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0 || value_size == 0) return PPDB_STORAGE_ERR_PARAM;

    // Put data using engine
    ppdb_error_t err = ppdb_engine_put(table->storage->current_tx, table->engine_table, key, key_size, value, value_size);
    if (err != PPDB_OK) {
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

    // Get data using engine
    ppdb_error_t err = ppdb_engine_get(table->storage->current_tx, table->engine_table, key, key_size, value, value_size);
    if (err != PPDB_OK) {
        return PPDB_STORAGE_ERR_NOT_FOUND;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_delete(ppdb_storage_table_t* table, const void* key, size_t key_size) {
    if (!table || !key) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0) return PPDB_STORAGE_ERR_PARAM;

    // Delete data using engine
    ppdb_error_t err = ppdb_engine_delete(table->storage->current_tx, table->engine_table, key, key_size);
    if (err != PPDB_OK) {
        return PPDB_STORAGE_ERR_NOT_FOUND;
    }

    // Update table size
    atomic_fetch_sub(&table->size, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan(ppdb_storage_table_t* table, ppdb_storage_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_STORAGE_ERR_PARAM;

    // Initialize cursor
    cursor->table = table;
    cursor->valid = false;

    // Open cursor in engine
    ppdb_error_t err = ppdb_engine_cursor_open(table->storage->current_tx, table->engine_table, &cursor->engine_cursor);
    if (err != PPDB_OK) {
        return err;
    }

    // Move cursor to first entry
    err = ppdb_engine_cursor_first(cursor->engine_cursor);
    if (err != PPDB_OK) {
        ppdb_engine_cursor_close(cursor->engine_cursor);
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

    // Compact table using engine
    ppdb_error_t err = ppdb_engine_compact(table->storage->current_tx, table->engine_table);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_storage_table_t* table) {
    if (!table) return PPDB_STORAGE_ERR_PARAM;

    // Flush table using engine
    ppdb_error_t err = ppdb_engine_flush(table->storage->current_tx, table->engine_table);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}
