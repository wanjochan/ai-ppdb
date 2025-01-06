//-----------------------------------------------------------------------------
// Storage Operations Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_storage_put(ppdb_storage_t* storage, const ppdb_data_t* key, const ppdb_data_t* value) {
    if (!storage || !key || !value) return PPDB_ERR_PARAM;

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement put operation
    // 1. Find active table
    // 2. Insert key-value pair
    // 3. Update statistics

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get(ppdb_storage_t* storage, const ppdb_data_t* key, ppdb_data_t* value) {
    if (!storage || !key || !value) return PPDB_ERR_PARAM;

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement get operation
    // 1. Find active table
    // 2. Search for key
    // 3. Update statistics

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_delete(ppdb_storage_t* storage, const ppdb_data_t* key) {
    if (!storage || !key) return PPDB_ERR_PARAM;

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement delete operation
    // 1. Find active table
    // 2. Delete key
    // 3. Update statistics

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan(ppdb_storage_t* storage, ppdb_storage_cursor_t* cursor) {
    if (!storage || !cursor) return PPDB_ERR_PARAM;

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement scan operation
    // 1. Find active table
    // 2. Initialize cursor
    // 3. Update statistics

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan_range(ppdb_storage_t* storage,
                                   const ppdb_data_t* start_key,
                                   const ppdb_data_t* end_key,
                                   ppdb_storage_cursor_t* cursor) {
    if (!storage || !start_key || !end_key || !cursor) return PPDB_ERR_PARAM;

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement range scan operation
    // 1. Find active table
    // 2. Initialize cursor with range
    // 3. Update statistics

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_compact(ppdb_storage_t* storage) {
    if (!storage) return PPDB_ERR_PARAM;

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement compaction
    // 1. Find active table
    // 2. Perform compaction
    // 3. Update statistics

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_storage_t* storage) {
    if (!storage) return PPDB_ERR_PARAM;

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement flush
    // 1. Find active table
    // 2. Flush data to disk
    // 3. Update statistics

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_checkpoint(ppdb_storage_t* storage) {
    if (!storage) return PPDB_ERR_PARAM;

    // Lock storage
    PPDB_RETURN_IF_ERROR(ppdb_base_spinlock_lock(&storage->lock));

    // TODO: Implement checkpoint
    // 1. Find active table
    // 2. Create checkpoint
    // 3. Update statistics

    ppdb_base_spinlock_unlock(&storage->lock);
    return PPDB_OK;
}
