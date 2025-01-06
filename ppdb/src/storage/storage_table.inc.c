//-----------------------------------------------------------------------------
// Table Management Implementation
//-----------------------------------------------------------------------------

static int table_name_compare(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

ppdb_error_t ppdb_table_create(ppdb_storage_t* storage, const char* name) {
    if (!storage || !name) return PPDB_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Check if table already exists
    ppdb_storage_table_t* table = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find((ppdb_base_skiplist_t*)storage->tables, name, (void**)&table);
    if (err == PPDB_OK && table != NULL) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_ERR_TABLE_EXISTS;
    }

    // Create new table
    table = malloc(sizeof(ppdb_storage_table_t));
    if (!table) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_ERR_MEMORY;
    }

    // Initialize table
    memset(table, 0, sizeof(ppdb_storage_table_t));
    table->name = strdup(name);
    if (!table->name) {
        free(table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_ERR_MEMORY;
    }

    // Initialize table lock
    err = ppdb_base_spinlock_init(&table->lock);
    if (err != PPDB_OK) {
        free(table->name);
        free(table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Initialize table data
    err = ppdb_base_skiplist_create(&table->data, table_name_compare);
    if (err != PPDB_OK) {
        free(table->name);
        free(table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Initialize table indexes
    err = ppdb_base_skiplist_create(&table->indexes, table_name_compare);
    if (err != PPDB_OK) {
        ppdb_base_skiplist_destroy(table->data);
        free(table->name);
        free(table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Add table to list
    err = ppdb_base_skiplist_insert((ppdb_base_skiplist_t*)storage->tables, name, table);
    if (err != PPDB_OK) {
        ppdb_base_skiplist_destroy(table->indexes);
        ppdb_base_skiplist_destroy(table->data);
        free(table->name);
        free(table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_table_drop(ppdb_storage_t* storage, const char* name) {
    if (!storage || !name) return PPDB_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Find table
    ppdb_storage_table_t* table = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find((ppdb_base_skiplist_t*)storage->tables, name, (void**)&table);
    if (err != PPDB_OK || table == NULL) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_ERR_TABLE_NOT_FOUND;
    }

    // Remove table from list
    err = ppdb_base_skiplist_remove((ppdb_base_skiplist_t*)storage->tables, name);
    if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Free table resources
    ppdb_base_skiplist_destroy(table->indexes);
    ppdb_base_skiplist_destroy(table->data);
    free(table->name);
    free(table);

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_table_open(ppdb_storage_t* storage, const char* name) {
    if (!storage || !name) return PPDB_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Find table
    ppdb_storage_table_t* table = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find((ppdb_base_skiplist_t*)storage->tables, 
                                              (const void*)name, 
                                              (void**)&table);
    if (err != PPDB_OK || table == NULL) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_ERR_TABLE_NOT_FOUND;
    }

    // TODO: Implement table opening logic

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_table_close(ppdb_storage_t* storage) {
    if (!storage) return PPDB_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement table closing logic

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table) {
    if (!storage || !name || !table) return PPDB_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Find table
    ppdb_error_t err = ppdb_base_skiplist_find((ppdb_base_skiplist_t*)storage->tables, name, (void**)table);
    if (err != PPDB_OK || *table == NULL) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_ERR_TABLE_NOT_FOUND;
    }

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}
