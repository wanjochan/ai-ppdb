/*
 * engine_io.inc.c - Engine IO Management Implementation
 */

// Forward declarations
static void ppdb_engine_io_thread(void* arg);

// IO initialization
ppdb_error_t ppdb_engine_io_init(ppdb_engine_t* engine) {
    if (!engine) return PPDB_ENGINE_ERR_PARAM;

    // Initialize IO manager
    engine->io_mgr.io_mgr = NULL;
    engine->io_mgr.io_thread = NULL;
    engine->io_mgr.io_running = false;

    // Create IO manager
    ppdb_error_t err = ppdb_base_io_manager_create(&engine->io_mgr.io_mgr);
    if (err != PPDB_OK) return err;

    // Start IO thread
    err = ppdb_base_thread_create(&engine->io_mgr.io_thread, ppdb_engine_io_thread, engine);
    if (err != PPDB_OK) {
        ppdb_base_io_manager_destroy(engine->io_mgr.io_mgr);
        engine->io_mgr.io_mgr = NULL;
        return err;
    }

    engine->io_mgr.io_running = true;
    return PPDB_OK;
}

// IO cleanup
void ppdb_engine_io_cleanup(ppdb_engine_t* engine) {
    if (!engine) return;

    // Stop IO thread
    if (engine->io_mgr.io_running) {
        engine->io_mgr.io_running = false;
        if (engine->io_mgr.io_thread) {
            ppdb_base_thread_join(engine->io_mgr.io_thread);
            ppdb_base_thread_destroy(engine->io_mgr.io_thread);
            engine->io_mgr.io_thread = NULL;
        }
    }

    // Cleanup IO manager
    if (engine->io_mgr.io_mgr) {
        ppdb_base_io_manager_destroy(engine->io_mgr.io_mgr);
        engine->io_mgr.io_mgr = NULL;
    }
}

// IO thread function
static void ppdb_engine_io_thread(void* arg) {
    ppdb_engine_t* engine = (ppdb_engine_t*)arg;
    while (engine->io_mgr.io_running) {
        // Process IO requests
        ppdb_base_io_manager_process(engine->io_mgr.io_mgr);
        ppdb_base_sleep(1);  // Sleep for 1ms to avoid busy waiting
    }
}

// Put operation
ppdb_error_t ppdb_engine_put(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                         const void* key, size_t key_size,
                         const void* value, size_t value_size) {
    if (!txn || !table || !key || !value) return PPDB_ENGINE_ERR_PARAM;
    if (key_size == 0 || value_size == 0) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->stats.is_active || txn->stats.is_committed || txn->stats.is_rolledback) {
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Check if table is open
    if (!table->is_open) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Allocate memory for key-value pair
    void* key_copy = malloc(key_size);
    void* value_copy = malloc(value_size);
    
    if (!key_copy || !value_copy) {
        if (key_copy) free(key_copy);
        if (value_copy) free(value_copy);
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }

    // Copy key and value
    memcpy(key_copy, key, key_size);
    memcpy(value_copy, value, value_size);

    // Create new entry
    ppdb_engine_entry_t* entry = malloc(sizeof(ppdb_engine_entry_t));
    if (!entry) {
        free(key_copy);
        free(value_copy);
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }

    // Initialize entry
    entry->key = key_copy;
    entry->key_size = key_size;
    entry->value = value_copy;
    entry->value_size = value_size;
    entry->next = NULL;

    // Add to table's entry list
    if (!table->entries) {
        table->entries = entry;
    } else {
        entry->next = table->entries;
        table->entries = entry;
    }

    // Update table statistics
    table->size++;
    ppdb_base_counter_inc(txn->stats.writes);

    // Unlock table
    ppdb_base_mutex_unlock(table->lock);

    return PPDB_OK;
}

// Get operation
ppdb_error_t ppdb_engine_get(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                         const void* key, size_t key_size,
                         void* value, size_t* value_size) {
    if (!txn || !table || !key || !value || !value_size) return PPDB_ENGINE_ERR_PARAM;
    if (key_size == 0) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Check if table is open
    if (!table->is_open) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Search for the key
    ppdb_engine_entry_t* entry = table->entries;
    while (entry) {
        if (entry->key_size == key_size && memcmp(entry->key, key, key_size) == 0) {
            // Found the key, check buffer size
            if (*value_size < entry->value_size) {
                *value_size = entry->value_size;
                ppdb_base_mutex_unlock(table->lock);
                return PPDB_ENGINE_ERR_BUFFER_FULL;
            }

            // Copy value
            memcpy(value, entry->value, entry->value_size);
            *value_size = entry->value_size;
            ppdb_base_counter_inc(txn->stats.reads);

            // Unlock table
            ppdb_base_mutex_unlock(table->lock);
            return PPDB_OK;
        }
        entry = entry->next;
    }

    // Key not found
    ppdb_base_mutex_unlock(table->lock);
    return PPDB_ENGINE_ERR_NOT_FOUND;
}

// Delete operation
ppdb_error_t ppdb_engine_delete(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                            const void* key, size_t key_size) {
    if (!txn || !table || !key) return PPDB_ENGINE_ERR_PARAM;
    if (key_size == 0) return PPDB_ENGINE_ERR_PARAM;
    if (!txn->stats.is_active) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Check if table is open
    if (!table->is_open) return PPDB_ENGINE_ERR_INVALID_STATE;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Search for the key
    ppdb_engine_entry_t** curr = &table->entries;
    while (*curr) {
        if ((*curr)->key_size == key_size && memcmp((*curr)->key, key, key_size) == 0) {
            // Found the key, remove it
            ppdb_engine_entry_t* entry = *curr;
            *curr = entry->next;

            // Free entry data
            free(entry->key);
            free(entry->value);
            free(entry);

            // Update statistics
            table->size--;
            ppdb_base_counter_inc(txn->stats.writes);

            // Unlock table
            ppdb_base_mutex_unlock(table->lock);
            return PPDB_OK;
        }
        curr = &(*curr)->next;
    }

    // Key not found
    ppdb_base_mutex_unlock(table->lock);
    return PPDB_ENGINE_ERR_NOT_FOUND;
}

// Maintenance operations
ppdb_error_t ppdb_engine_compact(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table) {
    if (!txn || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // TODO: Implement compaction

    // Unlock table
    ppdb_base_mutex_unlock(table->lock);

    return PPDB_OK;
}

ppdb_error_t ppdb_engine_flush(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table) {
    if (!txn || !table) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // TODO: Implement flush

    // Unlock table
    ppdb_base_mutex_unlock(table->lock);

    return PPDB_OK;
}