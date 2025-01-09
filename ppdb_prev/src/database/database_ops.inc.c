#include "internal/database.h"
#include "internal/base.h"

int ppdb_database_put(ppdb_database_t* db, ppdb_database_txn_t* txn,
                     const char* table_name, const void* key, size_t key_size,
                     const void* value, size_t value_size) {
    if (!db || !txn || !table_name || !key || !value) {
        return PPDB_ERR_PARAM;
    }

    // Get table
    ppdb_database_table_t* table = NULL;
    int ret = database_table_manager_get_table(db->table_manager, table_name, &table);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Check transaction status
    if (txn->status != PPDB_TXN_STATUS_ACTIVE) {
        return PPDB_DATABASE_ERR_TXN;
    }

    // Add to write set
    ret = database_write_set_add(txn->write_set, table, key, key_size, value, value_size);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Update stats
    ppdb_base_mutex_lock(&db->mutex);
    db->stats.write_ops++;
    db->stats.bytes_written += key_size + value_size;
    ppdb_base_mutex_unlock(&db->mutex);

    return PPDB_OK;
}

int ppdb_database_get(ppdb_database_t* db, ppdb_database_txn_t* txn,
                     const char* table_name, const void* key, size_t key_size,
                     void** value, size_t* value_size) {
    if (!db || !txn || !table_name || !key || !value || !value_size) {
        return PPDB_ERR_PARAM;
    }

    // Get table
    ppdb_database_table_t* table = NULL;
    int ret = database_table_manager_get_table(db->table_manager, table_name, &table);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Check transaction status
    if (txn->status != PPDB_TXN_STATUS_ACTIVE) {
        return PPDB_DATABASE_ERR_TXN;
    }

    // Check write set first
    ret = database_write_set_get(txn->write_set, table, key, key_size, value, value_size);
    if (ret == PPDB_OK) {
        // Update stats
        ppdb_base_mutex_lock(&db->mutex);
        db->stats.read_ops++;
        db->stats.write_set_hits++;
        db->stats.bytes_read += key_size + *value_size;
        ppdb_base_mutex_unlock(&db->mutex);
        return PPDB_OK;
    }

    // Check memtable
    uint64_t version;
    ret = database_memkv_get(table->memkv, key, key_size, value, value_size, &version);
    if (ret == PPDB_OK) {
        // Check visibility using MVCC
        if (!database_mvcc_is_visible(txn->snapshot, version)) {
            free(*value);
            return PPDB_DATABASE_ERR_CONFLICT;
        }

        // Add to read set
        ret = database_read_set_add(txn->read_set, table, key, key_size, version);
        if (ret != PPDB_OK) {
            free(*value);
            return ret;
        }

        // Update stats
        ppdb_base_mutex_lock(&db->mutex);
        db->stats.read_ops++;
        db->stats.memtable_hits++;
        db->stats.bytes_read += key_size + *value_size;
        ppdb_base_mutex_unlock(&db->mutex);
        return PPDB_OK;
    }

    // Not found
    ppdb_base_mutex_lock(&db->mutex);
    db->stats.read_ops++;
    db->stats.read_misses++;
    ppdb_base_mutex_unlock(&db->mutex);
    return PPDB_ERR_NOT_FOUND;
}

int ppdb_database_delete(ppdb_database_t* db, ppdb_database_txn_t* txn,
                        const char* table_name, const void* key, size_t key_size) {
    if (!db || !txn || !table_name || !key) {
        return PPDB_ERR_PARAM;
    }

    // Get table
    ppdb_database_table_t* table = NULL;
    int ret = database_table_manager_get_table(db->table_manager, table_name, &table);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Check transaction status
    if (txn->status != PPDB_TXN_STATUS_ACTIVE) {
        return PPDB_DATABASE_ERR_TXN;
    }

    // Add deletion marker to write set
    ret = database_write_set_add_deletion(txn->write_set, table, key, key_size);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Update stats
    ppdb_base_mutex_lock(&db->mutex);
    db->stats.delete_ops++;
    ppdb_base_mutex_unlock(&db->mutex);

   return PPDB_OK;
}

int ppdb_database_exists(ppdb_database_t* db, ppdb_database_txn_t* txn,
                        const char* table_name, const void* key, size_t key_size) {
    if (!db || !txn || !table_name || !key) {
        return PPDB_ERR_PARAM;
    }

    void* value = NULL;
    size_t value_size = 0;
    int ret = ppdb_database_get(db, txn, table_name, key, key_size, &value, &value_size);
    
    if (ret == PPDB_OK) {
        free(value);
        return 1;  // Exists
    } else if (ret == PPDB_ERR_NOT_FOUND) {
        return 0;  // Does not exist
    }
    
    return ret;  // Error
} 