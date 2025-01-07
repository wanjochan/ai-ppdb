//-----------------------------------------------------------------------------
// Table Management Implementation
//-----------------------------------------------------------------------------

// Data comparison function for storage
static int ppdb_storage_compare_data(const void* a, const void* b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    
    size_t size_a = *(const size_t*)a;
    size_t size_b = *(const size_t*)b;
    const char* data_a = (const char*)a + sizeof(size_t);
    const char* data_b = (const char*)b + sizeof(size_t);
    
    size_t min_size = size_a < size_b ? size_a : size_b;
    int result = memcmp(data_a, data_b, min_size);
    
    if (result != 0) return result;
    return size_a < size_b ? -1 : (size_a > size_b ? 1 : 0);
}

// Index comparison function for storage
static int ppdb_storage_compare_index(const void* a, const void* b) {
    if (!a || !b) return a ? 1 : (b ? -1 : 0);
    return strcmp((const char*)a, (const char*)b);
}

ppdb_error_t ppdb_storage_create_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table) {
    if (storage == NULL || name == NULL || table == NULL) {
        return PPDB_STORAGE_ERR_PARAM;
    }

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

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Check if table already exists
    ppdb_storage_table_t* existing_table = NULL;
    ppdb_error_t err = ppdb_storage_get_table(storage, name, &existing_table);
    if (err == PPDB_OK) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_TABLE_EXISTS;
    } else if (err != PPDB_STORAGE_ERR_TABLE_NOT_FOUND) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Create new table
    ppdb_storage_table_t* new_table = (ppdb_storage_table_t*)ppdb_base_aligned_alloc(16, sizeof(ppdb_storage_table_t));
    if (new_table == NULL) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Initialize table structure
    memset(new_table, 0, sizeof(ppdb_storage_table_t));

    // Copy table name
    new_table->name = (char*)ppdb_base_aligned_alloc(16, name_len + 1);
    if (new_table->name == NULL) {
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }
    memcpy(new_table->name, name, name_len + 1);
    new_table->name_len = name_len;

    // Create data skiplist
    err = ppdb_base_skiplist_create(&new_table->data, ppdb_storage_compare_data);
    if (err != PPDB_OK || new_table->data == NULL) {
        ppdb_base_aligned_free(new_table->name);
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Create indexes skiplist
    err = ppdb_base_skiplist_create(&new_table->indexes, ppdb_storage_compare_index);
    if (err != PPDB_OK || new_table->indexes == NULL) {
        ppdb_base_skiplist_destroy(new_table->data);
        ppdb_base_aligned_free(new_table->name);
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    // Initialize table lock
    err = ppdb_base_spinlock_init(&new_table->lock);
    if (err != PPDB_OK) {
        ppdb_base_skiplist_destroy(new_table->indexes);
        ppdb_base_skiplist_destroy(new_table->data);
        ppdb_base_aligned_free(new_table->name);
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    // Initialize table state
    new_table->size = 0;
    new_table->is_open = true;

    // Add table to storage
    err = ppdb_base_skiplist_insert(storage->tables, new_table->name, new_table);
    if (err != PPDB_OK) {
        ppdb_base_spinlock_destroy(&new_table->lock);
        ppdb_base_skiplist_destroy(new_table->indexes);
        ppdb_base_skiplist_destroy(new_table->data);
        ppdb_base_aligned_free(new_table->name);
        ppdb_base_aligned_free(new_table);
        ppdb_base_spinlock_unlock(&storage->lock);
        return err;
    }

    *table = new_table;
    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table) {
    if (!storage || !name || !table) return PPDB_STORAGE_ERR_PARAM;
    if (*table) return PPDB_STORAGE_ERR_PARAM;
    
    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // Find table using name
    void* found_table = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find(storage->tables, name, &found_table);
    if (err == PPDB_ERR_NOT_FOUND) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_TABLE_NOT_FOUND;
    } else if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_INTERNAL;
    }

    *table = (ppdb_storage_table_t*)found_table;
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

    // Find table using name
    void* found_table = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find(storage->tables, name, &found_table);
    if (err == PPDB_ERR_NOT_FOUND) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_TABLE_NOT_FOUND;
    } else if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_INTERNAL;
    }

    // Remove table from storage
    err = ppdb_base_skiplist_remove(storage->tables, name);
    if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&storage->lock);
        return PPDB_STORAGE_ERR_INTERNAL;
    }

    // Destroy table
    ppdb_storage_table_destroy((ppdb_storage_table_t*)found_table);

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}
