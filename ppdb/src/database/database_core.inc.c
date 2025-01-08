#include "internal/database.h"
#include "internal/base.h"

static int database_init_internal(ppdb_database_t* db) {
    if (!db) {
        return PPDB_ERR_PARAM;
    }

    // Initialize database components
    db->txn_manager = NULL;
    db->table_manager = NULL;
    db->index_manager = NULL;
    db->stats = (ppdb_database_stats_t){0};

    // Initialize locks
    ppdb_base_mutex_create(&db->mutex, NULL);
    ppdb_base_rwlock_create(&db->rwlock, NULL);

    return PPDB_OK;
}

static void database_cleanup_internal(ppdb_database_t* db) {
    if (!db) {
        return;
    }

    // Cleanup components
    if (db->txn_manager) {
        database_txn_manager_destroy(db->txn_manager);
    }
    if (db->table_manager) {
        database_table_manager_destroy(db->table_manager);
    }
    if (db->index_manager) {
        database_index_manager_destroy(db->index_manager);
    }

    // Cleanup locks
    ppdb_base_mutex_destroy(&db->mutex);
    ppdb_base_rwlock_destroy(&db->rwlock);
}

int ppdb_database_init(ppdb_database_t** db, ppdb_base_t* base) {
    if (!db || !base) {
        return PPDB_ERR_PARAM;
    }

    *db = (ppdb_database_t*)calloc(1, sizeof(ppdb_database_t));
    if (!*db) {
        return PPDB_ERR_NOMEM;
    }

    (*db)->base = base;
    int ret = database_init_internal(*db);
    if (ret != PPDB_OK) {
        free(*db);
        *db = NULL;
        return ret;
    }

    return PPDB_OK;
}

void ppdb_database_destroy(ppdb_database_t* db) {
    if (!db) {
        return;
    }

    database_cleanup_internal(db);
    free(db);
} 