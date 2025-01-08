//-----------------------------------------------------------------------------
// Database Operations Implementation
//-----------------------------------------------------------------------------

// Forward declarations
#ifndef PPDB_DATABASE_OPS_DECLARATIONS
#define PPDB_DATABASE_OPS_DECLARATIONS
static ppdb_error_t begin_write_transaction(ppdb_database_t* database);
static ppdb_error_t begin_read_transaction(ppdb_database_t* database);
static ppdb_error_t commit_transaction(ppdb_database_t* database);
static ppdb_error_t rollback_transaction(ppdb_database_t* database);
#endif

// Begin a write transaction
static ppdb_error_t begin_write_transaction(ppdb_database_t* database) {
    if (!database) return PPDB_DATABASE_ERR_PARAM;
    
    ppdb_error_t err = ppdb_database_txn_begin(database, true, &database->current_tx);
    if (err != PPDB_OK) return err;
    
    return PPDB_OK;
}

// Begin a read transaction
static ppdb_error_t begin_read_transaction(ppdb_database_t* database) {
   if (!database) return PPDB_DATABASE_ERR_PARAM;
    
    ppdb_error_t err = ppdb_database_txn_begin(database, false, &database->current_tx);
    if (err != PPDB_OK) return err;
    
    return PPDB_OK;
}

// Commit the current transaction
static ppdb_error_t commit_transaction(ppdb_database_t* database){
    if (!database) return PPDB_DATABASE_ERR_PARAM;

    ppdb_error_t err = ppdb_base_mutex_lock(database->lock);
    if (err != PPDB_OK) return err;

    if (!database->current_tx || !database->current_tx->stats.is_active) {
        ppdb_base_mutex_unlock(database->lock);
        return PPDB_DATABASE_ERR_INVALID_STATE;
    }

    err = ppdb_database_txn_commit(database->current_tx);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(database->lock);
        return err;
    }

    database->current_tx = NULL;
    ppdb_base_mutex_unlock(database->lock);
    return PPDB_OK;
}

// Rollback the current transaction
static ppdb_error_t rollback_transaction(ppdb_database_t* database) {
   if (!database) return PPDB_DATABASE_ERR_PARAM;

   ppdb_error_t err = ppdb_base_mutex_lock(database->lock);
    if (err != PPDB_OK) return err;

    if (!database->current_tx || !database->current_tx->stats.is_active) {
        ppdb_base_mutex_unlock(database->lock);
        return PPDB_DATABASE_ERR_INVALID_STATE;
    }

    err = ppdb_database_txn_rollback(database->current_tx);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(database->lock);
        return err;
    }

    database->current_tx = NULL;
    ppdb_base_mutex_unlock(database->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_database_put(ppdb_database_table_t* table, const void* key, size_t key_size,
                              const void* value, size_t value_size) {
    if (!table || !key || !value) return PPDB_DATABASE_ERR_PARAM;
    if (key_size == 0 || value_size == 0) return PPDB_DATABASE_ERR_PARAM;
    if (!table->database) return PPDB_DATABASE_ERR_PARAM;
    if (!table->db_table) return PPDB_DATABASE_ERR_INVALID_STATE;

    bool created_transaction = false;
    ppdb_error_t err = PPDB_OK;

    // Begin write transaction if needed
    if (!table->database->current_tx) {
        err = begin_write_transaction(table->database);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to begin write transaction (code: %d)\n", err);
            return err;
        }
        created_transaction = true;
    }

    // Put data using engine
    // Note: value_size should already include the null terminator
    err = ppdb_database_put(table->database->current_tx, table->db_table, key, key_size, value, value_size);
    if (err != PPDB_OK) {
        printf("ERROR: Failed to put data (code: %d)\n", err);
        if (created_transaction) {
            ppdb_error_t rb_err = rollback_transaction(table->database);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after put error (code: %d)\n", rb_err);
            }
        }
        return err;
    }

    // Commit the transaction only if we created it
    if (created_transaction) {
        err = commit_transaction(table->database);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to commit transaction (code: %d)\n", err);
            ppdb_error_t rb_err = rollback_transaction(table->database);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after commit error (code: %d)\n", rb_err);
            }
            return err;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_database_get(ppdb_database_table_t* table, const void*key, size_t key_size,
                             void* value, size_t* value_size) {
    if (!table || !key || !value || !value_size) return PPDB_DATABASE_ERR_PARAM;
   if (key_size == 0) return PPDB_DATABASE_ERR_PARAM;
    if (!table->database) return PPDB_DATABASE_ERR_PARAM;
    if (!table->db_table) return PPDB_DATABASE_ERR_INVALID_STATE;

    bool created_transaction = false;
    ppdb_error_t err = PPDB_OK;

    // Begin read transaction if needed
    if (!table->database->current_tx) {
        err = begin_read_transaction(table->database);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to begin read transaction (code: %d)\n", err);
            return err;
        }
        created_transaction = true;
    }

    // Get data using engine
    err = ppdb_database_get(table->database->current_tx, table->db_table, key, key_size, value, value_size);
    if (err != PPDB_OK) {
        if (err == PPDB_DATABASE_ERR_BUFFER_FULL) {
            if (created_transaction) {
                ppdb_error_t commit_err = commit_transaction(table->database);  // Clean up transaction
                if (commit_err != PPDB_OK) {
                    printf("ERROR: Failed to commit transaction after buffer full error (code: %d)\n", commit_err);
                }
            }
            return PPDB_DATABASE_ERR_BUFFER_FULL;
        } else if (err == PPDB_DATABASE_ERR_NOT_FOUND) {
            if (created_transaction) {
                ppdb_error_t commit_err = commit_transaction(table->database);  // Clean up transaction
                if (commit_err != PPDB_OK) {
                    printf("ERROR: Failed to commit transaction after not found error (code: %d)\n", commit_err);
                }
            }
            return PPDB_DATABASE_ERR_NOT_FOUND;
        }
        printf("ERROR: Failed to get data (code: %d)\n", err);
        if (created_transaction) {
            ppdb_error_t rb_err = rollback_transaction(table->database);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after get error (code: %d)\n", rb_err);
            }
        }
        return err;
    }

    // Commit the transaction only if we created it
    if (created_transaction) {
        err = commit_transaction(table->database);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to commit transaction (code: %d)\n", err);
            ppdb_error_t rb_err = rollback_transaction(table->database);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after commit error (code: %d)\n", rb_err);
            }
            return err;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_database_delete(ppdb_database_table_t* table, const void*key, size_t key_size) {
    if (!table || !key) return PPDB_DATABASE_ERR_PARAM;
   if (key_size == 0) return PPDB_DATABASE_ERR_PARAM;
    if (!table->database) return PPDB_DATABASE_ERR_PARAM;
    if (!table->db_table) return PPDB_DATABASE_ERR_INVALID_STATE;

    bool created_transaction = false;
    ppdb_error_t err = PPDB_OK;

    // Begin write transaction if needed
    if (!table->database->current_tx) {
        err = begin_write_transaction(table->database);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to begin write transaction (code: %d)\n", err);
            return err;
        }
        created_transaction = true;
    }

    // Delete data using engine
    err = ppdb_database_delete(table->database->current_tx, table->db_table, key, key_size);
    if (err != PPDB_OK) {
        if (err == PPDB_DATABASE_ERR_NOT_FOUND) {
            if (created_transaction) {
                ppdb_error_t commit_err = commit_transaction(table->database);  // Clean up transaction
                if (commit_err != PPDB_OK) {
                    printf("ERROR: Failed to commit transaction after not found error (code: %d)\n", commit_err);
                }
            }
            return PPDB_DATABASE_ERR_NOT_FOUND;
        }
        printf("ERROR: Failed to delete data (code: %d)\n", err);
        if (created_transaction) {
            ppdb_error_t rb_err = rollback_transaction(table->database);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after delete error (code: %d)\n", rb_err);
            }
        }
        return err;
    }

    // Commit the transaction only if we created it
    if (created_transaction) {
        err = commit_transaction(table->database);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to commit transaction (code: %d)\n", err);
            ppdb_error_t rb_err = rollback_transaction(table->database);
            if (rb_err != PPDB_OK) {
                printf("ERROR: Failed to rollback transaction after commit error (code: %d)\n", rb_err);
            }
            return err;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_database_scan(ppdb_database_table_t* table, ppdb_database_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_DATABASE_ERR_PARAM;

   // Begin transaction if needed
    ppdb_error_t err = begin_write_transaction(table->database);
    if (err != PPDB_OK) {
        return err;
    }

    // Initialize cursor
    cursor->table = table;
    cursor->valid = false;

    // Open cursor in engine
    err = ppdb_database_cursor_open(table->database->current_tx, table->db_table, &cursor->db_cursor);
    if (err != PPDB_OK) {
        rollback_transaction(table->database);
        return err;
    }

    // Move cursor to first entry
    err = ppdb_database_cursor_first(cursor->db_cursor);
    if (err != PPDB_OK) {
        ppdb_database_cursor_close(cursor->db_cursor);
        rollback_transaction(table->database);
        return err;
    }

    cursor->valid = true;
    return PPDB_OK;
}

ppdb_error_t ppdb_database_scan_next(ppdb_database_table_t* table, ppdb_database_cursor_t* cursor) {
    if (!table || !cursor) return PPDB_DATABASE_ERR_PARAM;
   if (!cursor->valid) return PPDB_DATABASE_ERR_INVALID_STATE;

    // Move cursor to next entry
    ppdb_error_t err = ppdb_database_cursor_next(cursor->db_cursor);
    if (err != PPDB_OK) {
        cursor->valid = false;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_database_compact(ppdb_database_table_t* table) {
    if (!table) return PPDB_DATABASE_ERR_PARAM;

    ppdb_error_t err = ppdb_database_table_compact(table->database->current_tx, table->db_table);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_database_flush(ppdb_database_table_t* table) {
    if (!table) return PPDB_DATABASE_ERR_PARAM;

    ppdb_error_t err = ppdb_database_table_flush(table->database->current_tx, table->db_table);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}
