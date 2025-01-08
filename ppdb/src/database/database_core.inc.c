/*
 * database_core.inc.c - Database Core Implementation
 */

// Database initialization
ppdb_error_t ppdb_database_init(ppdb_database_t** db, const ppdb_database_config_t* config) {
    if (!db || !config) return PPDB_DATABASE_ERR_START;

    ppdb_database_t* new_db = ppdb_base_malloc(sizeof(ppdb_database_t));
    if (!new_db) return PPDB_BASE_ERR_MEMORY;

    // Initialize configuration
    memcpy(&new_db->config, config, sizeof(ppdb_database_config_t));
    
    // Create mutex
    ppdb_error_t err = ppdb_base_mutex_create(&new_db->mutex);
    if (err != PPDB_OK) {
        ppdb_base_free(new_db);
        return err;
    }

    // Initialize MVCC if enabled
    if (config->enable_mvcc) {
        err = ppdb_mvcc_init(&new_db->mvcc);
        if (err != PPDB_OK) {
            ppdb_base_mutex_destroy(new_db->mutex);
            ppdb_base_free(new_db);
            return err;
        }
    }

    // Initialize storage
    err = ppdb_storage_init(&new_db->storage);
    if (err != PPDB_OK) {
        if (new_db->mvcc) ppdb_mvcc_destroy(new_db->mvcc);
        ppdb_base_mutex_destroy(new_db->mutex);
        ppdb_base_free(new_db);
        return err;
    }

    // Initialize statistics
    memset(&new_db->stats, 0, sizeof(ppdb_database_stats_t));
    new_db->initialized = true;
    *db = new_db;

    return PPDB_OK;
}

// Database cleanup
void ppdb_database_destroy(ppdb_database_t* db) {
    if (!db || !db->initialized) return;

    // Cleanup storage
    if (db->storage) {
        ppdb_storage_destroy(db->storage);
    }

    // Cleanup MVCC
    if (db->mvcc) {
        ppdb_mvcc_destroy(db->mvcc);
    }

    // Cleanup mutex
    if (db->mutex) {
        ppdb_base_mutex_destroy(db->mutex);
    }

    db->initialized = false;
    ppdb_base_free(db);
}

// Get database statistics
ppdb_error_t ppdb_database_get_stats(ppdb_database_t* db, ppdb_database_stats_t* stats) {
    if (!db || !db->initialized || !stats) return PPDB_DATABASE_ERR_START;

    ppdb_error_t err = ppdb_base_mutex_lock(db->mutex);
    if (err != PPDB_OK) return err;

    memcpy(stats, &db->stats, sizeof(ppdb_database_stats_t));

    err = ppdb_base_mutex_unlock(db->mutex);
    if (err != PPDB_OK) return err;

    return PPDB_OK;
}

// Internal: Update statistics
static ppdb_error_t database_update_stats(ppdb_database_t* db,
                                        ppdb_database_stats_t* delta) {
    if (!db || !db->initialized || !delta) return PPDB_DATABASE_ERR_START;

    ppdb_error_t err = ppdb_base_mutex_lock(db->mutex);
    if (err != PPDB_OK) return err;

    db->stats.total_txns += delta->total_txns;
    db->stats.committed_txns += delta->committed_txns;
    db->stats.aborted_txns += delta->aborted_txns;
    db->stats.conflicts += delta->conflicts;
    db->stats.deadlocks += delta->deadlocks;
    db->stats.cache_hits += delta->cache_hits;
    db->stats.cache_misses += delta->cache_misses;
    db->stats.bytes_written += delta->bytes_written;
    db->stats.bytes_read += delta->bytes_read;

    err = ppdb_base_mutex_unlock(db->mutex);
    if (err != PPDB_OK) return err;

    return PPDB_OK;
} 