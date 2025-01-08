/*
 * engine_table.inc.c - Engine Table Management Implementation
 */

// Table operations
ppdb_error_t ppdb_engine_table_create(ppdb_engine_txn_t* txn, const char* name,
                               ppdb_engine_table_t** table) {
    if (!txn || !name || !table) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->engine || !txn->engine->base) return PPDB_ENGINE_ERR_INVALID_STATE;
    if (*table) return PPDB_ENGINE_ERR_PARAM;  // Don't allow overwriting existing table
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock table list
    ppdb_error_t err = ppdb_base_mutex_lock(txn->engine->tables->lock);
    if (err != PPDB_OK) return err;

    // Check if table already exists
    ppdb_engine_table_t* existing = NULL;
    err = ppdb_engine_table_list_find(txn->engine->tables, name, &existing);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return err;
    }
    if (existing) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return PPDB_ENGINE_ERR_EXISTS;
    }

    // Allocate table structure
    ppdb_engine_table_t* new_table = malloc(sizeof(ppdb_engine_table_t));
    if (!new_table) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }

    // Initialize table structure
    memset(new_table, 0, sizeof(ppdb_engine_table_t));
    new_table->name = strdup(name);
    if (!new_table->name) {
        free(new_table);
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }
    new_table->name_len = strlen(name);
    new_table->engine = txn->engine;
    new_table->size = 0;
    new_table->is_open = true;
    new_table->entries = NULL;

    // Create table mutex
    err = ppdb_base_mutex_create(&new_table->lock);
    if (err != PPDB_OK) {
        free(new_table->name);
        free(new_table);
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return err;
    }

    // Add to table list
    err = ppdb_engine_table_list_add(txn->engine->tables, new_table);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(new_table->lock);
        free(new_table->name);
        free(new_table);
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return err;
    }

    // Unlock table list
    ppdb_base_mutex_unlock(txn->engine->tables->lock);

    *table = new_table;
    return PPDB_OK;
}

void ppdb_engine_table_destroy(ppdb_engine_table_t* table) {
    if (!table) return;

    // Lock table
    ppdb_base_mutex_lock(table->lock);

    // Mark table as closed
    table->is_open = false;

    // Cleanup table entries
    ppdb_engine_entry_t* entry = table->entries;
    while (entry) {
        ppdb_engine_entry_t* next = entry->next;
        if (entry->key) free(entry->key);
        if (entry->value) free(entry->value);
        free(entry);
        entry = next;
    }
    table->entries = NULL;
    table->size = 0;

    // Unlock and destroy table mutex
    ppdb_base_mutex_unlock(table->lock);
    ppdb_base_mutex_destroy(table->lock);

    // Free table name
    if (table->name) {
        free(table->name);
    }

    // Free table structure
    free(table);
}

ppdb_error_t ppdb_engine_table_open(ppdb_engine_txn_t* txn, const char* name,
                                ppdb_engine_table_t** table) {
    if (!txn || !name || !table) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->engine || !txn->engine->base) return PPDB_ENGINE_ERR_INVALID_STATE;
    if (*table) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock table list
    ppdb_error_t err = ppdb_base_mutex_lock(txn->engine->tables->lock);
    if (err != PPDB_OK) return err;

    // Find table
    ppdb_engine_table_t* existing = NULL;
    err = ppdb_engine_table_list_find(txn->engine->tables, name, &existing);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return err;
    }
    if (!existing) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return PPDB_ENGINE_ERR_NOT_FOUND;
    }

    // Lock table
    err = ppdb_base_mutex_lock(existing->lock);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return err;
    }

    // Check if table is already open
    if (existing->is_open) {
        ppdb_base_mutex_unlock(existing->lock);
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return PPDB_ENGINE_ERR_EXISTS;
    }

    // Mark table as open
    existing->is_open = true;

    // Unlock table
    ppdb_base_mutex_unlock(existing->lock);

    // Unlock table list
    ppdb_base_mutex_unlock(txn->engine->tables->lock);

    *table = existing;
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_close(ppdb_engine_table_t* table) {
    if (!table) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check if table is open
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Mark table as closed
    table->is_open = false;

    // Unlock table
    ppdb_base_mutex_unlock(table->lock);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_drop(ppdb_engine_txn_t* txn, const char* name) {
    if (!txn || !name) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->engine || !txn->engine->base) return PPDB_ENGINE_ERR_INVALID_STATE;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock table list
    ppdb_error_t err = ppdb_base_mutex_lock(txn->engine->tables->lock);
    if (err != PPDB_OK) return err;

    // Find table
    ppdb_engine_table_t* table = NULL;
    err = ppdb_engine_table_list_find(txn->engine->tables, name, &table);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return err;
    }
    if (!table) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return PPDB_ENGINE_ERR_NOT_FOUND;
    }

    // Lock table
    err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return err;
    }

    // Remove from table list
    err = ppdb_engine_table_list_remove(txn->engine->tables, name);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(table->lock);
        ppdb_base_mutex_unlock(txn->engine->tables->lock);
        return err;
    }

    // Destroy table
    ppdb_engine_table_destroy(table);

    // Unlock table list
    ppdb_base_mutex_unlock(txn->engine->tables->lock);

    return PPDB_OK;
}

uint64_t ppdb_engine_table_size(ppdb_engine_table_t* table) {
    if (!table) return 0;
    return table->size;
}

// Table maintenance operations
ppdb_error_t ppdb_engine_table_compact(ppdb_engine_table_t* table) {
    if (!table) return PPDB_ENGINE_ERR_PARAM;
    if (!table->is_open) return PPDB_ENGINE_ERR_INVALID_STATE;

    // TODO: Implement table compaction
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_cleanup_expired(ppdb_engine_table_t* table) {
    if (!table) return PPDB_ENGINE_ERR_PARAM;
    if (!table->is_open) return PPDB_ENGINE_ERR_INVALID_STATE;

    // TODO: Implement expired data cleanup
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_optimize_indexes(ppdb_engine_table_t* table) {
    if (!table) return PPDB_ENGINE_ERR_PARAM;
    if (!table->is_open) return PPDB_ENGINE_ERR_INVALID_STATE;

    // TODO: Implement index optimization
    return PPDB_OK;
} 