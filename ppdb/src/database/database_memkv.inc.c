/*
 * database_memkv.inc.c - Memory Storage Implementation
 */

// Storage initialization
ppdb_error_t ppdb_storage_init(ppdb_storage_t** storage) {
    if (!storage) return PPDB_DATABASE_ERR_STORAGE;

    ppdb_storage_t* new_storage = ppdb_base_malloc(sizeof(ppdb_storage_t));
    if (!new_storage) return PPDB_BASE_ERR_MEMORY;

    // Create memtable
    ppdb_error_t err = ppdb_base_skiplist_create(&new_storage->memtable,
                                                storage_key_compare);
    if (err != PPDB_OK) {
        ppdb_base_free(new_storage);
        return err;
    }

    // Create mutex
    err = ppdb_base_mutex_create(&new_storage->mutex);
    if (err != PPDB_OK) {
        ppdb_base_skiplist_destroy(new_storage->memtable);
        ppdb_base_free(new_storage);
        return err;
    }

    new_storage->data_dir = NULL;
    *storage = new_storage;
    return PPDB_OK;
}

// Storage cleanup
void ppdb_storage_destroy(ppdb_storage_t* storage) {
    if (!storage) return;

    if (storage->memtable) {
        ppdb_base_skiplist_destroy(storage->memtable);
    }

    if (storage->mutex) {
        ppdb_base_mutex_destroy(storage->mutex);
    }

    if (storage->data_dir) {
        ppdb_base_free(storage->data_dir);
    }

    ppdb_base_free(storage);
}

// Key comparison function
static int storage_key_compare(const void* a, const void* b) {
    const char* ka = (const char*)a;
    const char* kb = (const char*)b;
    return strcmp(ka, kb);
}

// Put key-value pair
ppdb_error_t ppdb_storage_put(ppdb_storage_t* storage, ppdb_txn_t* txn,
                             const void* key, size_t key_size,
                             const void* value, size_t value_size) {
    if (!storage || !txn || !key || !value) return PPDB_DATABASE_ERR_STORAGE;
    if (txn->flags & PPDB_TXN_READONLY) return PPDB_DATABASE_ERR_READONLY;

    ppdb_error_t err = ppdb_base_mutex_lock(storage->mutex);
    if (err != PPDB_OK) return err;

    // Insert into memtable
    err = ppdb_base_skiplist_insert(storage->memtable, key, key_size,
                                   value, value_size);

    // Update statistics
    if (err == PPDB_OK) {
        ppdb_database_stats_t delta = {0};
        delta.bytes_written = key_size + value_size;
        database_update_stats(txn->db, &delta);
    }

    ppdb_base_mutex_unlock(storage->mutex);
    return err;
}

// Get value by key
ppdb_error_t ppdb_storage_get(ppdb_storage_t* storage, ppdb_txn_t* txn,
                             const void* key, size_t key_size,
                             void** value, size_t* value_size) {
    if (!storage || !txn || !key || !value || !value_size) {
        return PPDB_DATABASE_ERR_STORAGE;
    }

    ppdb_error_t err = ppdb_base_mutex_lock(storage->mutex);
    if (err != PPDB_OK) return err;

    // Search in memtable
    err = ppdb_base_skiplist_find(storage->memtable, key, key_size,
                                 value, value_size);

    // Update statistics
    ppdb_database_stats_t delta = {0};
    if (err == PPDB_OK) {
        delta.cache_hits = 1;
        delta.bytes_read = key_size + *value_size;
    } else {
        delta.cache_misses = 1;
    }
    database_update_stats(txn->db, &delta);

    ppdb_base_mutex_unlock(storage->mutex);
    return err;
}

// Delete key-value pair
ppdb_error_t ppdb_storage_delete(ppdb_storage_t* storage, ppdb_txn_t* txn,
                                const void* key, size_t key_size) {
    if (!storage || !txn || !key) return PPDB_DATABASE_ERR_STORAGE;
    if (txn->flags & PPDB_TXN_READONLY) return PPDB_DATABASE_ERR_READONLY;

    ppdb_error_t err = ppdb_base_mutex_lock(storage->mutex);
    if (err != PPDB_OK) return err;

    // Remove from memtable
    err = ppdb_base_skiplist_remove(storage->memtable, key, key_size);

    ppdb_base_mutex_unlock(storage->mutex);
    return err;
}

// Sync storage to disk
ppdb_error_t ppdb_storage_sync(ppdb_storage_t* storage) {
    if (!storage) return PPDB_DATABASE_ERR_STORAGE;
    // Nothing to do for memory storage
    return PPDB_OK;
} 