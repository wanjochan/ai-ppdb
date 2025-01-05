#include <cosmopolitan.h>

static ppdb_version_t* create_version(ppdb_value_t* value, uint64_t txn_id, uint64_t ts) {
    ppdb_version_t* version = PPDB_ALIGNED_ALLOC(sizeof(ppdb_version_t));
    if (!version) return NULL;
    
    version->txn_id = txn_id;
    version->ts = ts;
    version->next = NULL;
    
    if (value) {
        version->value.size = value->size;
        version->value.data = PPDB_ALIGNED_ALLOC(value->size);
        if (!version->value.data) {
            PPDB_ALIGNED_FREE(version);
            return NULL;
        }
        memcpy(version->value.data, value->data, value->size);
    } else {
        version->value.size = 0;
        version->value.data = NULL;
    }
    
    return version;
}

static void destroy_version(ppdb_version_t* version) {
    if (!version) return;
    if (version->value.data) {
        PPDB_ALIGNED_FREE(version->value.data);
    }
    PPDB_ALIGNED_FREE(version);
}

static bool is_visible(ppdb_txn_t* txn, ppdb_version_t* version) {
    if (!version) return false;
    
    switch (txn->isolation) {
        case PPDB_ISOLATION_READ_UNCOMMITTED:
            return true;
            
        case PPDB_ISOLATION_READ_COMMITTED:
            return version->txn_id == txn->txn_id ||
                   (version->ts <= txn->start_ts);
            
        case PPDB_ISOLATION_REPEATABLE_READ:
        case PPDB_ISOLATION_SERIALIZABLE:
            return version->ts < txn->start_ts ||
                   version->txn_id == txn->txn_id;
    }
    
    return false;
}

ppdb_error_t ppdb_mvcc_get(ppdb_core_t* core, ppdb_txn_t* txn, 
                          ppdb_key_t* key, ppdb_value_t* value) {
    ppdb_mvcc_item_t* item = NULL;
    // Find item in storage (implementation depends on storage layer)
    ppdb_error_t err = ppdb_storage_get(core, txn, key, value);
    if (err != PPDB_OK) return err;
    
    ppdb_sync_lock(item->lock);
    
    // Find visible version
    ppdb_version_t* version = item->versions;
    while (version && !is_visible(txn, version)) {
        version = version->next;
    }
    
    if (!version) {
        ppdb_sync_unlock(item->lock);
        return PPDB_ERROR_NOT_FOUND;
    }
    
    // Copy value
    value->size = version->value.size;
    value->data = PPDB_ALIGNED_ALLOC(value->size);
    if (!value->data) {
        ppdb_sync_unlock(item->lock);
        return PPDB_ERROR_OOM;
    }
    memcpy(value->data, version->value.data, value->size);
    
    ppdb_sync_unlock(item->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_mvcc_put(ppdb_core_t* core, ppdb_txn_t* txn,
                          ppdb_key_t* key, ppdb_value_t* value) {
    ppdb_mvcc_item_t* item = NULL;
    // Find or create item in storage (implementation depends on storage layer)
    ppdb_error_t err = ppdb_storage_get(core, txn, key, NULL);
    if (err != PPDB_OK && err != PPDB_ERROR_NOT_FOUND) return err;
    
    ppdb_sync_lock(item->lock);
    
    // Create new version
    ppdb_version_t* version = create_version(value, txn->txn_id, txn->start_ts);
    if (!version) {
        ppdb_sync_unlock(item->lock);
        return PPDB_ERROR_OOM;
    }
    
    // Add to version chain
    version->next = item->versions;
    item->versions = version;
    
    ppdb_sync_unlock(item->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_mvcc_delete(ppdb_core_t* core, ppdb_txn_t* txn,
                             ppdb_key_t* key) {
    // Delete is implemented as a put with NULL value
    ppdb_value_t null_value = {0};
    return ppdb_mvcc_put(core, txn, key, &null_value);
}

static void cleanup_versions(ppdb_mvcc_item_t* item, uint64_t oldest_active_ts) {
    ppdb_sync_lock(item->lock);
    
    ppdb_version_t* curr = item->versions;
    ppdb_version_t* prev = NULL;
    
    while (curr && curr->next) {
        if (curr->next->ts < oldest_active_ts) {
            ppdb_version_t* to_delete = curr->next;
            curr->next = to_delete->next;
            destroy_version(to_delete);
        } else {
            curr = curr->next;
        }
    }
    
    ppdb_sync_unlock(item->lock);
} 