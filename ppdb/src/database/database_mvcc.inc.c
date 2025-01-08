/*
 * database_mvcc.inc.c - MVCC Implementation
 */

// MVCC version structure
typedef struct {
    uint64_t txn_id;
    void* data;
    size_t size;
    bool deleted;
} mvcc_version_t;

// MVCC initialization
ppdb_error_t ppdb_mvcc_init(ppdb_mvcc_t** mvcc) {
    if (!mvcc) return PPDB_DATABASE_ERR_MVCC;

    ppdb_mvcc_t* new_mvcc = ppdb_base_malloc(sizeof(ppdb_mvcc_t));
    if (!new_mvcc) return PPDB_BASE_ERR_MEMORY;

    // Initialize version counter
    atomic_store(&new_mvcc->next_txn_id, 1);

    // Create version skiplist
    ppdb_error_t err = ppdb_base_skiplist_create(&new_mvcc->versions,
                                                mvcc_version_compare);
    if (err != PPDB_OK) {
        ppdb_base_free(new_mvcc);
        return err;
    }

    // Create mutex
    err = ppdb_base_mutex_create(&new_mvcc->mutex);
    if (err != PPDB_OK) {
        ppdb_base_skiplist_destroy(new_mvcc->versions);
        ppdb_base_free(new_mvcc);
        return err;
    }

    *mvcc = new_mvcc;
    return PPDB_OK;
}

// MVCC cleanup
void ppdb_mvcc_destroy(ppdb_mvcc_t* mvcc) {
    if (!mvcc) return;

    if (mvcc->versions) {
        ppdb_base_skiplist_destroy(mvcc->versions);
    }

    if (mvcc->mutex) {
        ppdb_base_mutex_destroy(mvcc->mutex);
    }

    ppdb_base_free(mvcc);
}

// MVCC version comparison function
static int mvcc_version_compare(const void* a, const void* b) {
    const mvcc_version_t* va = (const mvcc_version_t*)a;
    const mvcc_version_t* vb = (const mvcc_version_t*)b;
    if (va->txn_id < vb->txn_id) return -1;
    if (va->txn_id > vb->txn_id) return 1;
    return 0;
}

// Add new version
ppdb_error_t ppdb_mvcc_add_version(ppdb_mvcc_t* mvcc, uint64_t txn_id,
                                  const void* data, size_t size) {
    if (!mvcc || !data) return PPDB_DATABASE_ERR_MVCC;

    mvcc_version_t* version = ppdb_base_malloc(sizeof(mvcc_version_t));
    if (!version) return PPDB_BASE_ERR_MEMORY;

    version->txn_id = txn_id;
    version->data = ppdb_base_malloc(size);
    if (!version->data) {
        ppdb_base_free(version);
        return PPDB_BASE_ERR_MEMORY;
    }

    memcpy(version->data, data, size);
    version->size = size;
    version->deleted = false;

    ppdb_error_t err = ppdb_base_mutex_lock(mvcc->mutex);
    if (err != PPDB_OK) {
        ppdb_base_free(version->data);
        ppdb_base_free(version);
        return err;
    }

    err = ppdb_base_skiplist_insert(mvcc->versions, version,
                                   sizeof(mvcc_version_t), NULL, 0);

    ppdb_base_mutex_unlock(mvcc->mutex);

    if (err != PPDB_OK) {
        ppdb_base_free(version->data);
        ppdb_base_free(version);
    }

    return err;
}

// Get version visible to transaction
ppdb_error_t ppdb_mvcc_get_version(ppdb_mvcc_t* mvcc, uint64_t txn_id,
                                  void** data, size_t* size) {
    if (!mvcc || !data || !size) return PPDB_DATABASE_ERR_MVCC;

    ppdb_error_t err = ppdb_base_mutex_lock(mvcc->mutex);
    if (err != PPDB_OK) return err;

    // Find the latest version visible to this transaction
    ppdb_base_skiplist_iterator_t* iter;
    err = ppdb_base_skiplist_iterator_create(mvcc->versions, &iter, true);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(mvcc->mutex);
        return err;
    }

    bool found = false;
    while (ppdb_base_skiplist_iterator_valid(iter)) {
        mvcc_version_t* version;
        size_t version_size;
        err = ppdb_base_skiplist_iterator_value(iter, (void**)&version,
                                              &version_size);
        if (err != PPDB_OK) break;

        if (version->txn_id <= txn_id && !version->deleted) {
            *data = ppdb_base_malloc(version->size);
            if (!*data) {
                err = PPDB_BASE_ERR_MEMORY;
                break;
            }
            memcpy(*data, version->data, version->size);
            *size = version->size;
            found = true;
            break;
        }

        err = ppdb_base_skiplist_iterator_next(iter);
        if (err != PPDB_OK) break;
    }

    ppdb_base_skiplist_iterator_destroy(iter);
    ppdb_base_mutex_unlock(mvcc->mutex);

    if (err != PPDB_OK) return err;
    return found ? PPDB_OK : PPDB_DATABASE_ERR_NOT_FOUND;
}

// Commit transaction's versions
ppdb_error_t ppdb_mvcc_commit(ppdb_mvcc_t* mvcc, ppdb_txn_t* txn) {
    if (!mvcc || !txn) return PPDB_DATABASE_ERR_MVCC;
    // Nothing to do for now, versions are already in place
    return PPDB_OK;
}

// Abort transaction's versions
ppdb_error_t ppdb_mvcc_abort(ppdb_mvcc_t* mvcc, ppdb_txn_t* txn) {
    if (!mvcc || !txn) return PPDB_DATABASE_ERR_MVCC;

    ppdb_error_t err = ppdb_base_mutex_lock(mvcc->mutex);
    if (err != PPDB_OK) return err;

    // Mark all versions from this transaction as deleted
    ppdb_base_skiplist_iterator_t* iter;
    err = ppdb_base_skiplist_iterator_create(mvcc->versions, &iter, false);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(mvcc->mutex);
        return err;
    }

    while (ppdb_base_skiplist_iterator_valid(iter)) {
        mvcc_version_t* version;
        size_t version_size;
        err = ppdb_base_skiplist_iterator_value(iter, (void**)&version,
                                              &version_size);
        if (err != PPDB_OK) break;

        if (version->txn_id == txn->txn_id) {
            version->deleted = true;
        }

        err = ppdb_base_skiplist_iterator_next(iter);
        if (err != PPDB_OK) break;
    }

    ppdb_base_skiplist_iterator_destroy(iter);
    ppdb_base_mutex_unlock(mvcc->mutex);

    return err;
} 