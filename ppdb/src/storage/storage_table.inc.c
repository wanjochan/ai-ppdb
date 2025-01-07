//-----------------------------------------------------------------------------
// Table Management Implementation
//-----------------------------------------------------------------------------

static int table_name_compare(const void* a, const void* b) {
    const ppdb_storage_table_t* table_a = (const ppdb_storage_table_t*)a;
    const ppdb_storage_table_t* table_b = (const ppdb_storage_table_t*)b;
    return strcmp(table_a->name, table_b->name);
}

ppdb_error_t ppdb_storage_create_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table) {
    if (!storage || !name || !table) return PPDB_STORAGE_ERR_PARAM;
    if (*table) return PPDB_STORAGE_ERR_PARAM;
    if (!name[0] || strspn(name, " \t\r\n") == strlen(name)) return PPDB_STORAGE_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Check if table already exists
    ppdb_storage_table_t* existing_table = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find((ppdb_base_skiplist_t*)storage->tables, name, (void**)&existing_table);
    if (err != PPDB_OK && err != PPDB_ERR_NOT_FOUND) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_INTERNAL;
    }
    if (existing_table != NULL) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_TABLE_EXISTS;
    }

    // Create new table
    ppdb_storage_table_t* new_table = ppdb_base_aligned_alloc(16, sizeof(ppdb_storage_table_t));
    if (!new_table) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Initialize table
    memset(new_table, 0, sizeof(ppdb_storage_table_t));
    size_t name_len = strlen(name) + 1;
    new_table->name = ppdb_base_aligned_alloc(16, name_len);
    if (!new_table->name) {
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }
    memcpy(new_table->name, name, name_len);

    // Initialize table lock
    err = ppdb_base_spinlock_init(&new_table->lock);
    if (err != PPDB_OK) {
        ppdb_base_aligned_free(new_table->name);
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Initialize table data
    err = ppdb_base_skiplist_create(&new_table->data, data_compare);
    if (err != PPDB_OK) {
        ppdb_base_spinlock_destroy(&new_table->lock);
        ppdb_base_aligned_free(new_table->name);
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Initialize table indexes
    err = ppdb_base_skiplist_create(&new_table->indexes, data_compare);
    if (err != PPDB_OK) {
        ppdb_base_skiplist_destroy(new_table->data);
        ppdb_base_spinlock_destroy(&new_table->lock);
        ppdb_base_aligned_free(new_table->name);
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Add table to storage
    err = ppdb_base_skiplist_insert((ppdb_base_skiplist_t*)storage->tables, new_table->name, new_table);
    if (err != PPDB_OK) {
        ppdb_base_skiplist_destroy(new_table->indexes);
        ppdb_base_skiplist_destroy(new_table->data);
        ppdb_base_spinlock_destroy(&new_table->lock);
        ppdb_base_aligned_free(new_table->name);
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    new_table->is_open = true;
    *table = new_table;
    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table) {
    if (!storage || !name || !table) return PPDB_STORAGE_ERR_PARAM;
    if (*table) return PPDB_STORAGE_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Find table
    ppdb_error_t err = ppdb_base_skiplist_find((ppdb_base_skiplist_t*)storage->tables, name, (void**)table);
    if (err != PPDB_OK || *table == NULL) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_TABLE_NOT_FOUND;
    }

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

void ppdb_storage_table_destroy(ppdb_storage_table_t* table) {
    if (!table) return;

    if (table->data) {
        ppdb_base_skiplist_destroy(table->data);
    }
    if (table->indexes) {
        ppdb_base_skiplist_destroy(table->indexes);
    }
    ppdb_base_spinlock_destroy(&table->lock);
    ppdb_base_aligned_free(table->name);
    ppdb_base_aligned_free(table);
}

ppdb_error_t ppdb_storage_drop_table(ppdb_storage_t* storage, const char* name) {
    if (!storage || !name) return PPDB_STORAGE_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Find table
    ppdb_storage_table_t* table = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find((ppdb_base_skiplist_t*)storage->tables, name, (void**)&table);
    if (err != PPDB_OK || table == NULL) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_TABLE_NOT_FOUND;
    }

    // Remove table from storage
    err = ppdb_base_skiplist_remove((ppdb_base_skiplist_t*)storage->tables, name);
    if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Destroy table
    ppdb_storage_table_destroy(table);

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}
