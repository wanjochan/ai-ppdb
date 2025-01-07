/*
 * engine_table.inc.c - Engine Table Management Implementation
 */

// Table operations
ppdb_error_t ppdb_engine_table_create(ppdb_engine_txn_t* txn, const char* name, ppdb_engine_table_t** table) {
    if (!txn || !name || !table) return PPDB_ENGINE_ERR_PARAM;
    if (*table) return PPDB_ENGINE_ERR_PARAM;

    // Create table structure
    ppdb_engine_table_t* new_table = malloc(sizeof(ppdb_engine_table_t));
    if (!new_table) return PPDB_ENGINE_ERR_INIT;

    // Initialize table structure
    memset(new_table, 0, sizeof(ppdb_engine_table_t));
    new_table->name = strdup(name);
    if (!new_table->name) {
        free(new_table);
        return PPDB_ENGINE_ERR_INIT;
    }
    new_table->name_len = strlen(name);
    new_table->engine = txn->engine;
    new_table->size = 0;
    new_table->is_open = true;

    // Create table lock
    ppdb_error_t err = ppdb_base_mutex_create(&new_table->lock);
    if (err != PPDB_OK) {
        free(new_table->name);
        free(new_table);
        return err;
    }

    *table = new_table;
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_open(ppdb_engine_txn_t* txn, const char* name, ppdb_engine_table_t** table) {
    if (!txn || !name || !table) return PPDB_ENGINE_ERR_PARAM;
    if (*table) return PPDB_ENGINE_ERR_PARAM;

    // Create table structure
    ppdb_engine_table_t* new_table = malloc(sizeof(ppdb_engine_table_t));
    if (!new_table) return PPDB_ENGINE_ERR_INIT;

    // Initialize table structure
    memset(new_table, 0, sizeof(ppdb_engine_table_t));
    new_table->name = strdup(name);
    if (!new_table->name) {
        free(new_table);
        return PPDB_ENGINE_ERR_INIT;
    }
    new_table->name_len = strlen(name);
    new_table->engine = txn->engine;
    new_table->size = 0;
    new_table->is_open = true;

    // Create table lock
    ppdb_error_t err = ppdb_base_mutex_create(&new_table->lock);
    if (err != PPDB_OK) {
        free(new_table->name);
        free(new_table);
        return err;
    }

    *table = new_table;
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_close(ppdb_engine_table_t* table) {
    if (!table) return PPDB_ENGINE_ERR_PARAM;
    if (!table->is_open) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Update table state
    table->is_open = false;

    // Unlock table
    ppdb_base_mutex_unlock(table->lock);

    // Cleanup table
    if (table->lock) {
        ppdb_base_mutex_destroy(table->lock);
    }
    if (table->name) {
        free(table->name);
    }
    free(table);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_table_drop(ppdb_engine_txn_t* txn, const char* name) {
    if (!txn || !name) return PPDB_ENGINE_ERR_PARAM;

    // TODO: Implement table drop
    return PPDB_OK;
}

uint64_t ppdb_engine_table_size(ppdb_engine_table_t* table) {
    if (!table) return 0;
    return table->size;
} 