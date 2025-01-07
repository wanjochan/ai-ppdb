//-----------------------------------------------------------------------------
// Table Management Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_storage_create_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table) {
    if (storage == NULL || name == NULL || table == NULL) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    printf("    Validating table name...\n");
    // Validate table name
    size_t name_len = strlen(name);
    if (name_len == 0) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    // Check if table name contains only whitespace
    bool only_whitespace = true;
    for (size_t i = 0; i < name_len; i++) {
        if (!isspace(name[i])) {
            only_whitespace = false;
            break;
        }
    }
    if (only_whitespace) {
        return PPDB_STORAGE_ERR_PARAM;
    }

    printf("    Checking if table exists...\n");
    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(storage->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Check if table exists
    ppdb_engine_table_t* existing_table = NULL;
    err = ppdb_engine_table_open(tx, name, &existing_table);
    if (err == PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return PPDB_STORAGE_ERR_TABLE_EXISTS;
    } else if (err != PPDB_ENGINE_ERR_NOT_FOUND) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    printf("    Creating table in engine...\n");
    // Create table in engine
    ppdb_engine_table_t* engine_table = NULL;
    err = ppdb_engine_table_create(tx, name, &engine_table);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    printf("    Allocating table structure...\n");
    // Create storage table wrapper
    ppdb_storage_table_t* new_table = malloc(sizeof(ppdb_storage_table_t));
    if (new_table == NULL) {
        ppdb_engine_txn_rollback(tx);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Initialize table structure
    new_table->name = strdup(name);
    if (new_table->name == NULL) {
        free(new_table);
        ppdb_engine_txn_rollback(tx);
        return PPDB_STORAGE_ERR_MEMORY;
    }
    new_table->name_len = name_len;
    new_table->engine_table = engine_table;
    new_table->engine = storage->engine;
    new_table->size = ppdb_engine_table_size(engine_table);
    new_table->is_open = true;

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        free(new_table->name);
        free(new_table);
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    *table = new_table;
    printf("    Table creation completed.\n");
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const void* name_key, ppdb_storage_table_t** table) {
    if (!storage || !name_key || !table) return PPDB_STORAGE_ERR_PARAM;
    if (*table) return PPDB_STORAGE_ERR_PARAM;

    // Begin read transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(storage->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Get table from engine
    ppdb_engine_table_t* engine_table = NULL;
    err = ppdb_engine_table_open(tx, name_key, &engine_table);
    if (err != PPDB_OK) {
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    // Create storage table wrapper
    ppdb_storage_table_t* new_table = malloc(sizeof(ppdb_storage_table_t));
    if (new_table == NULL) {
        ppdb_engine_txn_rollback(tx);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Initialize table structure
    new_table->name = strdup(name_key);
    if (new_table->name == NULL) {
        free(new_table);
        ppdb_engine_txn_rollback(tx);
        return PPDB_STORAGE_ERR_MEMORY;
    }
    new_table->name_len = strlen(name_key);
    new_table->engine_table = engine_table;
    new_table->engine = storage->engine;
    new_table->size = ppdb_engine_table_size(engine_table);
    new_table->is_open = true;

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        free(new_table->name);
        free(new_table);
        ppdb_engine_txn_rollback(tx);
        return err;
    }

    *table = new_table;
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_drop_table(ppdb_storage_t* storage, const char* name) {
    if (!storage || !name) return PPDB_STORAGE_ERR_PARAM;

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    ppdb_error_t err = ppdb_engine_txn_begin(storage->engine, &tx);
    if (err != PPDB_OK) {
        return err;
    }

    // Drop table in engine
    err = ppdb_engine_table_drop(tx, name);
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

void ppdb_storage_table_destroy(ppdb_storage_table_t* table) {
    if (table) {
        if (table->name) {
            free(table->name);
        }
        if (table->engine_table) {
            ppdb_engine_table_close(table->engine_table);
        }
        free(table);
    }
}
