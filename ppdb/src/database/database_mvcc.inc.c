#include "internal/database.h"
#include "internal/base.h"

typedef struct ppdb_database_snapshot {
    uint64_t txn_id;
    uint64_t min_active_txn_id;
    uint64_t max_committed_txn_id;
} ppdb_database_snapshot_t;

static int database_snapshot_create(ppdb_database_t* db, ppdb_database_snapshot_t** snapshot) {
    if (!db || !snapshot) {
        return PPDB_ERR_PARAM;
    }

    *snapshot = (ppdb_database_snapshot_t*)calloc(1, sizeof(ppdb_database_snapshot_t));
    if (!*snapshot) {
        return PPDB_ERR_NOMEM;
    }

    pthread_mutex_lock(&db->mutex);
    (*snapshot)->txn_id = db->stats.next_txn_id++;
    (*snapshot)->min_active_txn_id = db->stats.min_active_txn_id;
    (*snapshot)->max_committed_txn_id = db->stats.max_committed_txn_id;
    pthread_mutex_unlock(&db->mutex);

    return PPDB_OK;
}

static void database_snapshot_destroy(ppdb_database_snapshot_t* snapshot) {
    if (!snapshot) {
        return;
    }
    free(snapshot);
}

int ppdb_database_mvcc_begin_txn(ppdb_database_t* db, ppdb_database_txn_t* txn) {
    if (!db || !txn) {
        return PPDB_ERR_PARAM;
    }

    // Create snapshot for transaction
    ppdb_database_snapshot_t* snapshot = NULL;
    int ret = database_snapshot_create(db, &snapshot);
    if (ret != PPDB_OK) {
        return ret;
    }

    txn->snapshot = snapshot;
    return PPDB_OK;
}

int ppdb_database_mvcc_commit_txn(ppdb_database_t* db, ppdb_database_txn_t* txn) {
    if (!db || !txn) {
        return PPDB_ERR_PARAM;
    }

    pthread_mutex_lock(&db->mutex);
    db->stats.max_committed_txn_id = txn->snapshot->txn_id;
    pthread_mutex_unlock(&db->mutex);

    return PPDB_OK;
}

int ppdb_database_mvcc_rollback_txn(ppdb_database_t* db, ppdb_database_txn_t* txn) {
    if (!db || !txn) {
        return PPDB_ERR_PARAM;
    }

    // No special handling needed for rollback in MVCC
    return PPDB_OK;
}

bool ppdb_database_mvcc_is_visible(ppdb_database_snapshot_t* snapshot, uint64_t version) {
    if (!snapshot) {
        return false;
    }

    // Check if version is visible to this snapshot
    return version <= snapshot->max_committed_txn_id &&
           version < snapshot->txn_id;
} 