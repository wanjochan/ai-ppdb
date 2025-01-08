//-----------------------------------------------------------------------------
// Storage Operations Implementation
//-----------------------------------------------------------------------------

// Forward declarations
#ifndef PPDB_STORAGE_OPS_DECLARATIONS
#define PPDB_STORAGE_OPS_DECLARATIONS
static ppdb_error_t begin_write_transaction(ppdb_storage_t* storage);
static ppdb_error_t begin_read_transaction(ppdb_storage_t* storage);
static ppdb_error_t commit_transaction(ppdb_storage_t* storage);
static ppdb_error_t rollback_transaction(ppdb_storage_t* storage);
#endif

// Begin a write transaction
static ppdb_error_t begin_write_transaction(ppdb_storage_t* storage) {
    if (!storage) return PPDB_STORAGE_ERR_PARAM;
    
    ppdb_error_t err = ppdb_engine_txn_begin(storage->engine, true, &storage->current_tx);
    if (err != PPDB_OK) return err;
    
    return PPDB_OK;
}

// Begin a read transaction
static ppdb_error_t begin_read_transaction(ppdb_storage_t* storage) {
    if (!storage) return PPDB_STORAGE_ERR_PARAM;
    
    ppdb_error_t err = ppdb_engine_txn_begin(storage->engine, false, &storage->current_tx);
    if (err != PPDB_OK) return err;
    
    return PPDB_OK;
}

// Commit the current transaction
static ppdb_error_t commit_transaction(ppdb_storage_t* storage) {
    if (!storage) return PPDB_STORAGE_ERR_PARAM;

    // 获取存储层锁
    ppdb_error_t err = ppdb_base_mutex_lock(storage->lock);
    if (err != PPDB_OK) return err;

    // 验证事务状态
    if (!storage->current_tx || !storage->current_tx->stats.is_active) {
        ppdb_base_mutex_unlock(storage->lock);
        return PPDB_STORAGE_ERR_INVALID_STATE;
    }

    // 提交事务
    err = ppdb_engine_txn_commit(storage->current_tx);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(storage->lock);
        return err;
    }

    // 清理事务
    storage->current_tx = NULL;

    ppdb_base_mutex_unlock(storage->lock);
    return PPDB_OK;
}

// Rollback the current transaction
static ppdb_error_t rollback_transaction(ppdb_storage_t* storage) {
    if (!storage) return PPDB_STORAGE_ERR_PARAM;

    // 获取存储层锁
    ppdb_error_t err = ppdb_base_mutex_lock(storage->lock);
    if (err != PPDB_OK) return err;

    // 验证事务状态
    if (!storage->current_tx || !storage->current_tx->stats.is_active) {
        ppdb_base_mutex_unlock(storage->lock);
        return PPDB_STORAGE_ERR_INVALID_STATE;
    }

    // 回滚事务
    err = ppdb_engine_txn_rollback(storage->current_tx);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(storage->lock);
        return err;
    }

    // 清理事务
    storage->current_tx = NULL;

    ppdb_base_mutex_unlock(storage->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_put(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             const void* value, size_t value_size) {
    if (!table || !key || !value) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0 || value_size == 0) return PPDB_STORAGE_ERR_PARAM;
    if (!table->storage) return PPDB_STORAGE_ERR_PARAM;
    if (!table->engine_table) return PPDB_STORAGE_ERR_INVALID_STATE;

    bool created_transaction = false;
    ppdb_error_t err = PPDB_OK;

    // Begin write transaction if needed
    if (!table->storage->current_tx) {
        err = begin_write_transaction(table->storage);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to begin write transaction (code: %d)\n", err);
            return err;
        }
        created_transaction = true;
    }

    // Put data using engine
    // Note: value_size should already include the null terminator
    err = ppdb_engine_put(table->storage->current_tx, table->engine_table, key, key_size, value, value_size);
    if (err != PPDB_OK) {
        printf("ERROR: Failed to put data (code: %d)\n", err);
        if (created_transaction) {
            ppdb_error_t rb_err = rollback_transaction(table->storage);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after put error (code: %d)\n", rb_err);
            }
        }
        return err;
    }

    // Commit the transaction only if we created it
    if (created_transaction) {
        err = commit_transaction(table->storage);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to commit transaction (code: %d)\n", err);
            ppdb_error_t rb_err = rollback_transaction(table->storage);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after commit error (code: %d)\n", rb_err);
            }
            return err;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get(ppdb_storage_table_t* table, const void* key, size_t key_size,
                             void* value, size_t* value_size) {
    if (!table || !key || !value || !value_size) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0) return PPDB_STORAGE_ERR_PARAM;
    if (!table->storage) return PPDB_STORAGE_ERR_PARAM;
    if (!table->engine_table) return PPDB_STORAGE_ERR_INVALID_STATE;

    bool created_transaction = false;
    ppdb_error_t err = PPDB_OK;

    // Begin read transaction if needed
    if (!table->storage->current_tx) {
        err = begin_read_transaction(table->storage);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to begin read transaction (code: %d)\n", err);
            return err;
        }
        created_transaction = true;
    }

    // Get data using engine
    err = ppdb_engine_get(table->storage->current_tx, table->engine_table, key, key_size, value, value_size);
    if (err != PPDB_OK) {
        if (err == PPDB_ENGINE_ERR_BUFFER_FULL) {
            if (created_transaction) {
                ppdb_error_t commit_err = commit_transaction(table->storage);  // Clean up transaction
                if (commit_err != PPDB_OK) {
                    printf("ERROR: Failed to commit transaction after buffer full error (code: %d)\n", commit_err);
                }
            }
            return PPDB_STORAGE_ERR_BUFFER_FULL;
        } else if (err == PPDB_ENGINE_ERR_NOT_FOUND) {
            if (created_transaction) {
                ppdb_error_t commit_err = commit_transaction(table->storage);  // Clean up transaction
                if (commit_err != PPDB_OK) {
                    printf("ERROR: Failed to commit transaction after not found error (code: %d)\n", commit_err);
                }
            }
            return PPDB_STORAGE_ERR_NOT_FOUND;
        }
        printf("ERROR: Failed to get data (code: %d)\n", err);
        if (created_transaction) {
            ppdb_error_t rb_err = rollback_transaction(table->storage);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after get error (code: %d)\n", rb_err);
            }
        }
        return err;
    }

    // Commit the transaction only if we created it
    if (created_transaction) {
        err = commit_transaction(table->storage);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to commit transaction (code: %d)\n", err);
            ppdb_error_t rb_err = rollback_transaction(table->storage);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after commit error (code: %d)\n", rb_err);
            }
            return err;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_delete(ppdb_storage_table_t* table, const void* key, size_t key_size) {
    if (!table || !key) return PPDB_STORAGE_ERR_PARAM;
    if (key_size == 0) return PPDB_STORAGE_ERR_PARAM;
    if (!table->storage) return PPDB_STORAGE_ERR_PARAM;
    if (!table->engine_table) return PPDB_STORAGE_ERR_INVALID_STATE;

    bool created_transaction = false;
    ppdb_error_t err = PPDB_OK;

    // Begin write transaction if needed
    if (!table->storage->current_tx) {
        err = begin_write_transaction(table->storage);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to begin write transaction (code: %d)\n", err);
            return err;
        }
        created_transaction = true;
    }

    // Delete data using engine
    err = ppdb_engine_delete(table->storage->current_tx, table->engine_table, key, key_size);
    if (err != PPDB_OK) {
        if (err == PPDB_ENGINE_ERR_NOT_FOUND) {
            if (created_transaction) {
                ppdb_error_t commit_err = commit_transaction(table->storage);  // Clean up transaction
                if (commit_err != PPDB_OK) {
                    printf("ERROR: Failed to commit transaction after not found error (code: %d)\n", commit_err);
                }
            }
            return PPDB_STORAGE_ERR_NOT_FOUND;
        }
        printf("ERROR: Failed to delete data (code: %d)\n", err);
        if (created_transaction) {
            ppdb_error_t rb_err = rollback_transaction(table->storage);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after delete error (code: %d)\n", rb_err);
            }
        }
        return err;
    }

    // Commit the transaction only if we created it
    if (created_transaction) {
        err = commit_transaction(table->storage);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to commit transaction (code: %d)\n", err);
            ppdb_error_t rb_err = rollback_transaction(table->storage);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after commit error (code: %d)\n", rb_err);
            }
            return err;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan(ppdb_storage_table_t* table, ppdb_storage_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_STORAGE_ERR_PARAM;

    // Begin transaction if needed
    ppdb_error_t err = begin_write_transaction(table->storage);
    if (err != PPDB_OK) {
        return err;
    }

    // Initialize cursor
    cursor->table = table;
    cursor->valid = false;

    // Open cursor in engine
    err = ppdb_engine_cursor_open(table->storage->current_tx, table->engine_table, &cursor->engine_cursor);
    if (err != PPDB_OK) {
        rollback_transaction(table->storage);
        return err;
    }

    // Move cursor to first entry
    err = ppdb_engine_cursor_first(cursor->engine_cursor);
    if (err != PPDB_OK) {
        ppdb_engine_cursor_close(cursor->engine_cursor);
        rollback_transaction(table->storage);
        return err;
    }

    cursor->valid = true;
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_scan_next(ppdb_storage_table_t* table, ppdb_storage_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_STORAGE_ERR_PARAM;
    if (!cursor->valid) return PPDB_STORAGE_ERR_INVALID_STATE;

    // Move cursor to next entry
    ppdb_error_t err = ppdb_engine_cursor_next(cursor->engine_cursor);
    if (err != PPDB_OK) {
        cursor->valid = false;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_compact(ppdb_storage_table_t* table) {
    if (!table) return PPDB_STORAGE_ERR_PARAM;

    // Compact table using engine
    ppdb_error_t err = ppdb_engine_compact(table->storage->current_tx, table->engine_table);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_storage_table_t* table) {
    if (!table) return PPDB_STORAGE_ERR_PARAM;

    // Flush table using engine
    ppdb_error_t err = ppdb_engine_flush(table->storage->current_tx, table->engine_table);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}
