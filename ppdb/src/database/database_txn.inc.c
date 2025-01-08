#include "internal/database.h"
#include "internal/base.h"

static int database_txn_init_internal(ppdb_database_txn_t* txn, ppdb_database_t* db) {
    if (!txn || !db) {
        return PPDB_ERR_PARAM;
    }

    txn->db = db;
    txn->status = PPDB_TXN_STATUS_ACTIVE;
    txn->isolation_level = PPDB_TXN_ISOLATION_SERIALIZABLE;
    txn->stats = (ppdb_database_txn_stats_t){0};

    // Initialize transaction locks
    pthread_mutex_init(&txn->mutex, NULL);
    pthread_rwlock_init(&txn->rwlock, NULL);

    // Initialize transaction components
    txn->write_set = NULL;
    txn->read_set = NULL;
    txn->snapshot = NULL;

    return PPDB_OK;
}

static void database_txn_cleanup_internal(ppdb_database_txn_t* txn) {
    if (!txn) {
        return;
    }

    // Cleanup transaction components
    if (txn->write_set) {
        database_write_set_destroy(txn->write_set);
    }
    if (txn->read_set) {
        database_read_set_destroy(txn->read_set);
    }
    if (txn->snapshot) {
        database_snapshot_destroy(txn->snapshot);
    }

    // Cleanup locks
    pthread_mutex_destroy(&txn->mutex);
    pthread_rwlock_destroy(&txn->rwlock);
}

int ppdb_database_txn_begin(ppdb_database_t* db, ppdb_database_txn_t** txn) {
    if (!db || !txn) {
        return PPDB_ERR_PARAM;
    }

    *txn = (ppdb_database_txn_t*)calloc(1, sizeof(ppdb_database_txn_t));
    if (!*txn) {
        return PPDB_ERR_NOMEM;
    }

    int ret = database_txn_init_internal(*txn, db);
    if (ret != PPDB_OK) {
        free(*txn);
        *txn = NULL;
        return ret;
    }

    // Update database stats
    pthread_mutex_lock(&db->mutex);
    db->stats.active_txns++;
    pthread_mutex_unlock(&db->mutex);

    return PPDB_OK;
}

int ppdb_database_txn_commit(ppdb_database_txn_t* txn) {
    if (!txn || !txn->db) {
        return PPDB_ERR_PARAM;
    }

    if (txn->status != PPDB_TXN_STATUS_ACTIVE) {
        return PPDB_DATABASE_ERR_TXN;
    }

    // Commit changes
    int ret = database_write_set_apply(txn->write_set);
    if (ret != PPDB_OK) {
        return ret;
    }

    txn->status = PPDB_TXN_STATUS_COMMITTED;

    // Update stats
    pthread_mutex_lock(&txn->db->mutex);
    txn->db->stats.committed_txns++;
    txn->db->stats.active_txns--;
    pthread_mutex_unlock(&txn->db->mutex);

    return PPDB_OK;
}

int ppdb_database_txn_rollback(ppdb_database_txn_t* txn) {
    if (!txn || !txn->db) {
        return PPDB_ERR_PARAM;
    }

    if (txn->status != PPDB_TXN_STATUS_ACTIVE) {
        return PPDB_DATABASE_ERR_TXN;
    }

    txn->status = PPDB_TXN_STATUS_ABORTED;

    // Update stats
    pthread_mutex_lock(&txn->db->mutex);
    txn->db->stats.aborted_txns++;
    txn->db->stats.active_txns--;
    pthread_mutex_unlock(&txn->db->mutex);

    return PPDB_OK;
}

void ppdb_database_txn_destroy(ppdb_database_txn_t* txn) {
    if (!txn) {
        return;
    }

    database_txn_cleanup_internal(txn);
    free(txn);
} 