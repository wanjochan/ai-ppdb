/*
 * engine_io.inc.c - Engine IO implementation
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Initialize IO system
ppdb_error_t ppdb_engine_io_init(ppdb_engine_t* engine) {
    if (!engine) return PPDB_ENGINE_ERR_PARAM;

    // Create IO manager
    ppdb_error_t err = ppdb_base_io_manager_create(&engine->io_mgr.io_mgr);
    if (err != PPDB_OK) return err;

    // Initialize IO thread
    engine->io_mgr.io_thread = NULL;
    engine->io_mgr.io_running = false;

    return PPDB_OK;
}

// Cleanup IO system
void ppdb_engine_io_cleanup(ppdb_engine_t* engine) {
    if (!engine) return;

    // Stop IO thread if running
    if (engine->io_mgr.io_running) {
        engine->io_mgr.io_running = false;
        if (engine->io_mgr.io_thread) {
            ppdb_base_thread_join(engine->io_mgr.io_thread);
            ppdb_base_thread_destroy(engine->io_mgr.io_thread);
            engine->io_mgr.io_thread = NULL;
        }
    }

    // Destroy IO manager
    if (engine->io_mgr.io_mgr) {
        ppdb_base_io_manager_destroy(engine->io_mgr.io_mgr);
        engine->io_mgr.io_mgr = NULL;
    }
}

// Get value from table
ppdb_error_t ppdb_engine_get(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                            const void* key, size_t key_size,
                            void* value, size_t* value_size) {
    if (!txn || !table || !key || !key_size || !value || !value_size) {
        return PPDB_ENGINE_ERR_PARAM;
    }

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Find entry
    ppdb_engine_entry_t* entry = table->entries;
    while (entry) {
        if (entry->key_len == key_size && memcmp(entry->key, key, key_size) == 0) {
            // Check buffer size
            if (*value_size < entry->value_len) {
                *value_size = entry->value_len;
                ppdb_base_mutex_unlock(table->lock);
                return PPDB_ENGINE_ERR_BUFFER_FULL;
            }

            // Copy value
            memcpy(value, entry->value, entry->value_len);
            *value_size = entry->value_len;

            // Update statistics
            txn->stats.read_count++;

            ppdb_base_mutex_unlock(table->lock);
            return PPDB_OK;
        }
        entry = entry->next;
    }

    ppdb_base_mutex_unlock(table->lock);
    return PPDB_ENGINE_ERR_NOT_FOUND;
}

// Put value into table
ppdb_error_t ppdb_engine_put(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                            const void* key, size_t key_size,
                            const void* value, size_t value_size) {
    if (!txn || !table || !key || !key_size || !value || !value_size) {
        return PPDB_ENGINE_ERR_PARAM;
    }

    // Check transaction state
    if (!txn->is_write) {
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Find existing entry
    ppdb_engine_entry_t* entry = table->entries;
    while (entry) {
        if (entry->key_len == key_size && memcmp(entry->key, key, key_size) == 0) {
            // Add rollback record
            err = ppdb_engine_txn_add_rollback(txn, PPDB_ENGINE_ROLLBACK_PUT,
                                             table, key, key_size,
                                             entry->value, entry->value_len);
            if (err != PPDB_OK) {
                ppdb_base_mutex_unlock(table->lock);
                return err;
            }

            // Update existing entry
            void* new_value = malloc(value_size);
            if (!new_value) {
                ppdb_base_mutex_unlock(table->lock);
                return PPDB_ENGINE_ERR_MEMORY;
            }
            memcpy(new_value, value, value_size);
            free(entry->value);
            entry->value = new_value;
            entry->value_len = value_size;

            // Update statistics
            txn->stats.write_count++;

            ppdb_base_mutex_unlock(table->lock);
            return PPDB_OK;
        }
        entry = entry->next;
    }

    // Create new entry
    ppdb_engine_entry_t* new_entry = malloc(sizeof(ppdb_engine_entry_t));
    if (!new_entry) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }

    // Copy key
    new_entry->key = malloc(key_size);
    if (!new_entry->key) {
        free(new_entry);
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }
    memcpy(new_entry->key, key, key_size);
    new_entry->key_len = key_size;

    // Copy value
    new_entry->value = malloc(value_size);
    if (!new_entry->value) {
        free(new_entry->key);
        free(new_entry);
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_MEMORY;
    }
    memcpy(new_entry->value, value, value_size);
    new_entry->value_len = value_size;

    // Add rollback record
    err = ppdb_engine_txn_add_rollback(txn, PPDB_ENGINE_ROLLBACK_DELETE,
                                      table, key, key_size, NULL, 0);
    if (err != PPDB_OK) {
        free(new_entry->value);
        free(new_entry->key);
        free(new_entry);
        ppdb_base_mutex_unlock(table->lock);
        return err;
    }

    // Add to table
    new_entry->next = table->entries;
    table->entries = new_entry;
    table->size++;

    // Update statistics
    txn->stats.write_count++;

    ppdb_base_mutex_unlock(table->lock);
    return PPDB_OK;
}

// Delete value from table
ppdb_error_t ppdb_engine_delete(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                               const void* key, size_t key_size) {
    if (!txn || !table || !key || !key_size) {
        return PPDB_ENGINE_ERR_PARAM;
    }

    // Check transaction state
    if (!txn->is_write) {
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Find entry
    ppdb_engine_entry_t* prev = NULL;
    ppdb_engine_entry_t* entry = table->entries;
    while (entry) {
        if (entry->key_len == key_size && memcmp(entry->key, key, key_size) == 0) {
            // Add rollback record
            err = ppdb_engine_txn_add_rollback(txn, PPDB_ENGINE_ROLLBACK_PUT,
                                             table, key, key_size,
                                             entry->value, entry->value_len);
            if (err != PPDB_OK) {
                ppdb_base_mutex_unlock(table->lock);
                return err;
            }

            // Remove entry
            if (prev) {
                prev->next = entry->next;
            } else {
                table->entries = entry->next;
            }
            free(entry->key);
            free(entry->value);
            free(entry);
            table->size--;

            // Update statistics
            txn->stats.delete_count++;

            ppdb_base_mutex_unlock(table->lock);
            return PPDB_OK;
        }
        prev = entry;
        entry = entry->next;
    }

    ppdb_base_mutex_unlock(table->lock);
    return PPDB_ENGINE_ERR_NOT_FOUND;
}

// Check if key exists in table
ppdb_error_t ppdb_engine_exists(ppdb_engine_table_t* table, const void* key, size_t key_len) {
    if (!table || !key || !key_len) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Find entry
    ppdb_engine_entry_t* entry = table->entries;
    while (entry) {
        if (entry->key_len == key_len && memcmp(entry->key, key, key_len) == 0) {
            ppdb_base_mutex_unlock(table->lock);
            return PPDB_OK;
        }
        entry = entry->next;
    }

    ppdb_base_mutex_unlock(table->lock);
    return PPDB_ENGINE_ERR_NOT_FOUND;
}

// Get value size from table
ppdb_error_t ppdb_engine_get_size(ppdb_engine_table_t* table, const void* key, size_t key_len,
                                 size_t* value_len) {
    if (!table || !key || !key_len || !value_len) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    // Find entry
    ppdb_engine_entry_t* entry = table->entries;
    while (entry) {
        if (entry->key_len == key_len && memcmp(entry->key, key, key_len) == 0) {
            *value_len = entry->value_len;
            ppdb_base_mutex_unlock(table->lock);
            return PPDB_OK;
        }
        entry = entry->next;
    }

    ppdb_base_mutex_unlock(table->lock);
    return PPDB_ENGINE_ERR_NOT_FOUND;
}

// Get table size
ppdb_error_t ppdb_engine_get_table_size(ppdb_engine_table_t* table, size_t* size) {
    if (!table || !size) return PPDB_ENGINE_ERR_PARAM;

    // Lock table
    ppdb_error_t err = ppdb_base_mutex_lock(table->lock);
    if (err != PPDB_OK) return err;

    // Check table state
    if (!table->is_open) {
        ppdb_base_mutex_unlock(table->lock);
        return PPDB_ENGINE_ERR_INVALID_STATE;
    }

    *size = table->size;
    ppdb_base_mutex_unlock(table->lock);
    return PPDB_OK;
}