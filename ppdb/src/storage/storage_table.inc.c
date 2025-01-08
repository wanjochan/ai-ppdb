//-----------------------------------------------------------------------------
// Table Management Implementation
//-----------------------------------------------------------------------------

// Forward declarations from storage_ops.inc.c
static ppdb_error_t begin_write_transaction(ppdb_storage_t* storage);
static ppdb_error_t commit_transaction(ppdb_storage_t* storage);
static ppdb_error_t rollback_transaction(ppdb_storage_t* storage);

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

    // Begin transaction if needed
    if (!storage->current_tx) {
        ppdb_error_t err = begin_write_transaction(storage);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to begin transaction for table creation: %d\n", err);
            return err;
        }
    }

    // Verify transaction state
    if (!storage->current_tx || !storage->current_tx->stats.is_active) {
        printf("ERROR: Invalid transaction state for table creation\n");
        return PPDB_STORAGE_ERR_INVALID_STATE;
    }

    printf("    Creating table in engine...\n");
    // Create table in engine
    ppdb_engine_table_t* engine_table = NULL;
    ppdb_error_t err = ppdb_engine_table_create(storage->current_tx, name, &engine_table);
    if (err != PPDB_OK) {
        printf("ERROR: Failed to create table in engine: %d\n", err);
        return err;
    }

    // Verify engine table creation
    if (!engine_table) {
        printf("ERROR: Engine table creation failed but no error was returned\n");
        return PPDB_STORAGE_ERR_INVALID_STATE;
    }

    printf("    Allocating table structure...\n");
    // Create storage table wrapper
    ppdb_storage_table_t* new_table = malloc(sizeof(ppdb_storage_table_t));
    if (new_table == NULL) {
        printf("ERROR: Failed to allocate memory for table structure\n");
        ppdb_engine_table_close(engine_table);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Initialize table structure
    new_table->name = strdup(name);
    if (new_table->name == NULL) {
        printf("ERROR: Failed to allocate memory for table name\n");
        ppdb_engine_table_close(engine_table);
        free(new_table);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    new_table->name_len = name_len;
    new_table->engine_table = engine_table;
    new_table->engine = storage->engine;
    new_table->storage = storage;
    new_table->size = 0;
    new_table->is_open = true;

    *table = new_table;
    printf("    Table creation completed.\n");

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const void* name_key, ppdb_storage_table_t** table) {
    if (!storage || !name_key || !table) return PPDB_STORAGE_ERR_PARAM;
    if (*table) return PPDB_STORAGE_ERR_PARAM;

    // Begin transaction if needed
    if (!storage->current_tx) {
        ppdb_error_t err = begin_write_transaction(storage);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // Open table in engine
    ppdb_engine_table_t* engine_table = NULL;
    ppdb_error_t err = ppdb_engine_table_open(storage->current_tx, name_key, &engine_table);
    if (err != PPDB_OK) {
        rollback_transaction(storage);
        return err;
    }

    // Create storage table wrapper
    ppdb_storage_table_t* new_table = malloc(sizeof(ppdb_storage_table_t));
    if (new_table == NULL) {
        ppdb_engine_table_close(engine_table);
        rollback_transaction(storage);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Initialize with explicit storage reference
    new_table->name = strdup(name_key);
    if (new_table->name == NULL) {
        ppdb_engine_table_close(engine_table);
        free(new_table);
        rollback_transaction(storage);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    new_table->name_len = strlen(name_key);
    new_table->engine_table = engine_table;
    new_table->engine = storage->engine;
    new_table->storage = storage;
    new_table->size = ppdb_engine_table_size(engine_table);
    new_table->is_open = true;

    *table = new_table;

    // Note: We don't commit the transaction here, let the caller decide when to commit
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_drop_table(ppdb_storage_t* storage, const char* name) {
    if (!storage || !name) return PPDB_STORAGE_ERR_PARAM;

    // Begin transaction if needed
    if (!storage->current_tx) {
        ppdb_error_t err = begin_write_transaction(storage);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // Drop table in engine
    ppdb_error_t err = ppdb_engine_table_drop(storage->current_tx, name);
    if (err != PPDB_OK) {
        rollback_transaction(storage);
        return err;
    }

    // Note: We don't commit the transaction here, let the caller decide when to commit
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
