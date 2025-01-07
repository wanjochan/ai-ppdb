//-----------------------------------------------------------------------------
// Storage Operations Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_storage_put(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             const void* value, size_t value_size) {
    if (!table || !key || !value) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0 || value_size == 0) return PPDB_STORAGE_ERR_PARAM;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // Create copies of key and value
    void* key_copy = ppdb_base_aligned_alloc(16, sizeof(size_t) + key_size);
    void* value_copy = ppdb_base_aligned_alloc(16, sizeof(size_t) + value_size);
    if (!key_copy || !value_copy) {
        if (key_copy) ppdb_base_aligned_free(key_copy);
        if (value_copy) ppdb_base_aligned_free(value_copy);
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }

    *(size_t*)key_copy = key_size;
    memcpy((char*)key_copy + sizeof(size_t), key, key_size);
    *(size_t*)value_copy = value_size;
    memcpy((char*)value_copy + sizeof(size_t), value, value_size);

    // Insert into skiplist
    ppdb_error_t err = ppdb_base_skiplist_insert(table->data, key_copy, value_copy);
    if (err != PPDB_OK) {
        ppdb_base_aligned_free(key_copy);
        ppdb_base_aligned_free(value_copy);
        ppdb_base_spinlock_unlock(&table->lock);
        return err;
    }

    table->size++;
    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             void* value, size_t* value_size) {
    if (!table || !key || !value || !value_size) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0) return PPDB_STORAGE_ERR_PARAM;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // Create temporary key copy for lookup
    void* key_copy = ppdb_base_aligned_alloc(16, sizeof(size_t) + key_size);
    if (!key_copy) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }
    *(size_t*)key_copy = key_size;
    memcpy((char*)key_copy + sizeof(size_t), key, key_size);

    // Find value in skiplist
    void* value_ptr = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find(table->data, key_copy, &value_ptr);
    if (err == PPDB_ERR_PARAM) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_PARAM;
    } else if (err == PPDB_ERR_NOT_FOUND) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_NOT_FOUND;
    } else if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_INTERNAL;
    }

    // Copy value
    size_t found_size = *(size_t*)value_ptr;
    if (*value_size < found_size) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_PARAM;
    }

    memcpy(value, (char*)value_ptr + sizeof(size_t), found_size);
    *value_size = found_size;

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_delete(ppdb_storage_table_t* table, const void* key, size_t key_size) {
    if (!table || !key) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0) return PPDB_STORAGE_ERR_PARAM;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // Create temporary key copy for lookup
    void* key_copy = ppdb_base_aligned_alloc(16, sizeof(size_t) + key_size);
    if (!key_copy) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_MEMORY;
    }
    *(size_t*)key_copy = key_size;
    memcpy((char*)key_copy + sizeof(size_t), key, key_size);

    // Remove from skiplist
    ppdb_error_t err = ppdb_base_skiplist_remove(table->data, key_copy);
    ppdb_base_aligned_free(key_copy);

    if (err == PPDB_ERR_NOT_FOUND) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_NOT_FOUND;
    } else if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_STORAGE_ERR_INTERNAL;
    }

    table->size--;
    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan(ppdb_storage_table_t* table, ppdb_storage_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_ERR_NULL_POINTER;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // Initialize cursor
    cursor->table = table;
    cursor->current = NULL;
    cursor->valid = false;

    // TODO: Implement scan operation

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan_next(ppdb_storage_table_t* table, ppdb_storage_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_ERR_NULL_POINTER;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // TODO: Implement scan next operation

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_compact(ppdb_storage_table_t* table) {
    if (!table) return PPDB_ERR_NULL_POINTER;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // TODO: Implement compaction operation

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_storage_table_t* table) {
    if (!table) return PPDB_ERR_NULL_POINTER;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // TODO: Implement flush operation

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}
