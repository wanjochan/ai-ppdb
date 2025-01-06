//-----------------------------------------------------------------------------
// Storage Operations Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_storage_put(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             const void* value, size_t value_size) {
    if (!table || !key || !value) return PPDB_ERR_PARAM;
    
    // Avoid unused parameter warnings
    (void)key_size;
    (void)value_size;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // Insert into skiplist
    ppdb_error_t err = ppdb_base_skiplist_insert(table->data, key, (void*)value);
    if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&table->lock);
        return err;
    }

    table->size++;
    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             void* value, size_t* value_size) {
    if (!table || !key || !value || !value_size) return PPDB_ERR_PARAM;
    
    // Avoid unused parameter warnings
    (void)key_size;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // Find in skiplist
    void* found_value = NULL;
    ppdb_error_t err = ppdb_base_skiplist_find(table->data, key, &found_value);
    if (err != PPDB_OK || found_value == NULL) {
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_ERR_NOT_FOUND;
    }

    // Copy value
    size_t len = strlen((const char*)found_value);
    if (len + 1 > *value_size) {  // +1 for null terminator
        ppdb_base_spinlock_unlock(&table->lock);
        return PPDB_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(value, found_value, len);
    ((char*)value)[len] = '\0';  // Add null terminator
    *value_size = len;

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_delete(ppdb_storage_table_t* table, const void* key, size_t key_size) {
    if (!table || !key) return PPDB_ERR_PARAM;
    
    // Avoid unused parameter warnings
    (void)key_size;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // Remove from skiplist
    ppdb_error_t err = ppdb_base_skiplist_remove(table->data, key);
    if (err != PPDB_OK) {
        ppdb_base_spinlock_unlock(&table->lock);
        return err;
    }

    table->size--;
    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan(ppdb_storage_table_t* table, ppdb_storage_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_ERR_PARAM;
    
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
    if (!table || !cursor) return PPDB_ERR_PARAM;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // TODO: Implement scan next operation

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_compact(ppdb_storage_table_t* table) {
    if (!table) return PPDB_ERR_PARAM;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // TODO: Implement compaction operation

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_storage_table_t* table) {
    if (!table) return PPDB_ERR_PARAM;
    
    // Lock table
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&table->lock));

    // TODO: Implement flush operation

    ppdb_base_spinlock_unlock(&table->lock);
    return PPDB_OK;
}
