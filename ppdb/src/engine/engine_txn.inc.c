/*
 * database_txn.inc.c - Database transaction implementation
 */

#include <cosmopolitan.h>
#include "internal/database.h"

// Initialize transaction system
ppdb_error_t ppdb_database_txn_init(ppdb_database_t* db) {
    if (!db) return PPDB_DATABASE_ERR_PARAM;

    // Initialize transaction manager
    ppdb_error_t err = ppdb_base_mutex_create(&db->txn_mgr.lock);
    if (err != PPDB_OK) return err;

    db->txn_mgr.next_txn_id = 1;
    db->txn_mgr.active_txns = NULL;

    return PPDB_OK;
}

// Cleanup transaction system
void ppdb_database_txn_cleanup(ppdb_database_t* db) {
    if (!db) return;

    // Rollback any active transactions
    ppdb_database_txn_t* txn = db->txn_mgr.active_txns;
    while (txn) {
        ppdb_database_txn_t* next = txn->next;
        ppdb_database_txn_rollback(txn);
        txn = next;
    }

    // Cleanup transaction manager
    if (db->txn_mgr.lock) {
        ppdb_base_mutex_destroy(db->txn_mgr.lock);
        db->txn_mgr.lock = NULL;
    }
}

// Begin a new transaction
ppdb_error_t ppdb_database_txn_begin(ppdb_database_t* db, ppdb_database_txn_t** txn) {
    if (!db || !txn) return PPDB_DATABASE_ERR_PARAM;

    // Lock transaction manager
    ppdb_error_t err = ppdb_base_mutex_lock(db->txn_mgr.lock);
    if (err != PPDB_OK) return err;

    // Allocate transaction structure
    ppdb_database_txn_t* new_txn = malloc(sizeof(ppdb_database_txn_t));
    if (!new_txn) {
        ppdb_base_mutex_unlock(db->txn_mgr.lock);
        return PPDB_DATABASE_ERR_MEMORY;
    }

    // Initialize transaction
    memset(new_txn, 0, sizeof(ppdb_database_txn_t));
    new_txn->db = db;
    new_txn->id = db->txn_mgr.next_txn_id++;
    new_txn->rollback_records = NULL;
    new_txn->rollback_count = 0;
    new_txn->next = NULL;
    new_txn->is_write = false;

    // Create transaction mutex
    err = ppdb_base_mutex_create(&new_txn->lock);
    if (err != PPDB_OK) {
        free(new_txn);
        ppdb_base_mutex_unlock(db->txn_mgr.lock);
        return err;
    }

    // Initialize transaction statistics
    memset(&new_txn->stats, 0, sizeof(ppdb_database_txn_stats_t));

    // Add to active transactions list
    new_txn->next = db->txn_mgr.active_txns;
    db->txn_mgr.active_txns = new_txn;

    ppdb_base_mutex_unlock(db->txn_mgr.lock);
    *txn = new_txn;
    return PPDB_OK;
}

// Commit a transaction
ppdb_error_t ppdb_database_txn_commit(ppdb_database_txn_t* txn) {
    if (!txn || !txn->db) return PPDB_DATABASE_ERR_PARAM;

    // Locktransaction
    ppdb_error_t err = ppdb_base_mutex_lock(txn->lock);
    if (err != PPDB_OK) return err;

    // Lock transaction manager
    err = ppdb_base_mutex_lock(txn->db->txn_mgr.lock);
    if (err !=PPDB_OK) {
        ppdb_base_mutex_unlock(txn->lock);
        return err;
    }

    // Remove from active transactions list
    ppdb_database_txn_t** curr = &txn->db->txn_mgr.active_txns;
    while (*curr) {
        if (*curr == txn) {
            *curr = txn->next;
            break;
        }
        curr = &(*curr)->next;
    }

    // Free rollback records
    ppdb_database_rollback_record_t* record = txn->rollback_records;
    while (record) {
        ppdb_database_rollback_record_t* next = record->next;
        if (record->key) free(record->key);
        if (record->data) free(record->data);
        free(record);
        record = next;
    }

    // Cleanup transaction
    ppdb_base_mutex_unlock(txn->db->txn_mgr.lock);
    ppdb_base_mutex_unlock(txn->lock);
    ppdb_base_mutex_destroy(txn->lock);
    free(txn);

    return PPDB_OK;
}

// Rollback a transaction
ppdb_error_t ppdb_database_txn_rollback(ppdb_database_txn_t* txn) {
    if (!txn || !txn->db) return PPDB_DATABASE_ERR_PARAM;

    // Locktransaction
    ppdb_error_t err = ppdb_base_mutex_lock(txn->lock);
    if (err != PPDB_OK) return err;

    // Lock transaction manager
    err = ppdb_base_mutex_lock(txn->db->txn_mgr.lock);
    if (err !=PPDB_OK) {
        ppdb_base_mutex_unlock(txn->lock);
        return err;
    }

    // Process rollback records in reverse order
    ppdb_database_rollback_record_t* record = txn->rollback_records;
    while (record) {
        ppdb_database_rollback_record_t* next = record->next;

        // Apply rollback operation
        switch (record->type) {
            case PPDB_DATABASE_ROLLBACK_PUT:
                if (record->data) {
                    err = ppdb_database_put(record->table, record->key, record->key_size,
                                        record->data, record->value_size);
                }
                break;

            case PPDB_DATABASE_ROLLBACK_DELETE:
                err = ppdb_database_delete(record->table, record->key, record->key_size);
                break;
        }

        // Free rollback record
        if (record->key) free(record->key);
        if (record->data) free(record->data);
        free(record);
        record = next;
    }

    // Remove from active transactions list
    ppdb_database_txn_t** curr = &txn->db->txn_mgr.active_txns;
    while (*curr) {
        if (*curr == txn) {
            *curr = txn->next;
            break;
        }
        curr = &(*curr)->next;
    }

    // Cleanup transaction
    ppdb_base_mutex_unlock(txn->db->txn_mgr.lock);
    ppdb_base_mutex_unlock(txn->lock);
    ppdb_base_mutex_destroy(txn->lock);
    free(txn);

    return PPDB_OK;
}

// Add a rollback record
ppdb_error_t ppdb_database_txn_add_rollback(ppdb_database_txn_t* txn,
                                           ppdb_database_rollback_type_t type,
                                           ppdb_database_table_t* table,
                                           const void* key, size_t key_size,
                                           const void* value, size_t value_size) {
    if (!txn || !table || !key || key_size == 0) return PPDB_DATABASE_ERR_PARAM;

    // Locktransaction
    ppdb_error_t err = ppdb_base_mutex_lock(txn->lock);
    if (err != PPDB_OK) return err;

    // Allocate rollback record
    ppdb_database_rollback_record_t* record = malloc(sizeof(ppdb_database_rollback_record_t));
    if (!record) {
        ppdb_base_mutex_unlock(txn->lock);
        return PPDB_DATABASE_ERR_MEMORY;
    }

    // Initialize record
    record->type = type;
    record->table = table;
    record->key = malloc(key_size);
    if (!record->key) {
        free(record);
        ppdb_base_mutex_unlock(txn->lock);
        return PPDB_DATABASE_ERR_MEMORY;
    }
    memcpy(record->key, key, key_size);
    record->key_size = key_size;

    // Copy value if provided
    if (value && value_size > 0) {
        record->data = malloc(value_size);
        if (!record->data) {
            free(record->key);
            free(record);
            ppdb_base_mutex_unlock(txn->lock);
            return PPDB_DATABASE_ERR_MEMORY;
        }
        memcpy(record->data, value, value_size);
        record->value_size = value_size;
    } else {
        record->data = NULL;
        record->value_size = 0;
    }

    // Add to rollback list
    record->next = txn->rollback_records;
    txn->rollback_records = record;
    txn->rollback_count++;

    ppdb_base_mutex_unlock(txn->lock);
    return PPDB_OK;
}

// Get transaction statistics
void ppdb_database_txn_get_stats(ppdb_database_txn_t* txn, ppdb_database_txn_stats_t* stats) {
    if (!txn || !stats) return;

    // Lock transaction
    ppdb_error_t err = ppdb_base_mutex_lock(txn->lock);
    if (err != PPDB_OK) return;

    // Copy statistics
    memcpy(stats, &txn->stats, sizeof(ppdb_database_txn_stats_t));

    ppdb_base_mutex_unlock(txn->lock);
}