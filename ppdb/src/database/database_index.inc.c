/*
 * database_index.inc.c - Index Management Implementation
 */

// Index initialization
static ppdb_error_t index_init(ppdb_index_t** index, const char* name,
                              ppdb_base_compare_func_t compare) {
    if (!index || !name || !compare) return PPDB_DATABASE_ERR_INDEX;

    ppdb_index_t* new_index = ppdb_base_malloc(sizeof(ppdb_index_t));
    if (!new_index) return PPDB_BASE_ERR_MEMORY;

    new_index->name = ppdb_base_malloc(strlen(name) + 1);
    if (!new_index->name) {
        ppdb_base_free(new_index);
        return PPDB_BASE_ERR_MEMORY;
    }
    strcpy(new_index->name, name);

    // Create skiplist
    ppdb_error_t err = ppdb_base_skiplist_create(&new_index->tree, compare);
    if (err != PPDB_OK) {
        ppdb_base_free(new_index->name);
        ppdb_base_free(new_index);
        return err;
    }

    // Create mutex
    err = ppdb_base_mutex_create(&new_index->mutex);
    if (err != PPDB_OK) {
        ppdb_base_skiplist_destroy(new_index->tree);
        ppdb_base_free(new_index->name);
        ppdb_base_free(new_index);
        return err;
    }

    new_index->compare = compare;
    *index = new_index;
    return PPDB_OK;
}

// Index cleanup
static void index_destroy(ppdb_index_t* index) {
    if (!index) return;

    if (index->tree) {
        ppdb_base_skiplist_destroy(index->tree);
    }

    if (index->mutex) {
        ppdb_base_mutex_destroy(index->mutex);
    }

    if (index->name) {
        ppdb_base_free(index->name);
    }

    ppdb_base_free(index);
}

// Create index
ppdb_error_t ppdb_index_create(ppdb_txn_t* txn, const char* name,
                              ppdb_base_compare_func_t compare) {
    if (!txn || !name || !compare) return PPDB_DATABASE_ERR_INDEX;
    if (txn->flags & PPDB_TXN_READONLY) return PPDB_DATABASE_ERR_READONLY;

    ppdb_index_t* index;
    ppdb_error_t err = index_init(&index, name, compare);
    if (err != PPDB_OK) return err;

    // TODO: Store index in database's index map
    // For now, we just create and destroy it

    index_destroy(index);
    return PPDB_OK;
}

// Drop index
ppdb_error_t ppdb_index_drop(ppdb_txn_t* txn, const char* name) {
    if (!txn || !name) return PPDB_DATABASE_ERR_INDEX;
    if (txn->flags & PPDB_TXN_READONLY) return PPDB_DATABASE_ERR_READONLY;

    // TODO: Remove index from database's index map
    return PPDB_OK;
}

// Get value from index
ppdb_error_t ppdb_index_get(ppdb_txn_t* txn, const char* name,
                           const void* key, size_t key_size,
                           void** value, size_t* value_size) {
    if (!txn || !name || !key || !value || !value_size) {
        return PPDB_DATABASE_ERR_INDEX;
    }

    // TODO: Get index from database's index map
    return PPDB_DATABASE_ERR_NOT_FOUND;
}

// Create iterator
ppdb_error_t ppdb_iterator_create(ppdb_txn_t* txn, const char* index_name,
                                 ppdb_iterator_t** iterator) {
    if (!txn || !index_name || !iterator) return PPDB_DATABASE_ERR_INDEX;

    ppdb_iterator_t* new_iter = ppdb_base_malloc(sizeof(ppdb_iterator_t));
    if (!new_iter) return PPDB_BASE_ERR_MEMORY;

    new_iter->txn = txn;
    new_iter->index = NULL;  // TODO: Get from database's index map
    new_iter->iter = NULL;
    new_iter->valid = false;

    *iterator = new_iter;
    return PPDB_OK;
}

// Destroy iterator
ppdb_error_t ppdb_iterator_destroy(ppdb_iterator_t* iterator) {
    if (!iterator) return PPDB_DATABASE_ERR_INDEX;

    if (iterator->iter) {
        ppdb_base_skiplist_iterator_destroy(iterator->iter);
    }

    ppdb_base_free(iterator);
    return PPDB_OK;
}

// Seek to key
ppdb_error_t ppdb_iterator_seek(ppdb_iterator_t* iterator,
                               const void* key, size_t key_size) {
    if (!iterator || !iterator->index || !key) {
        return PPDB_DATABASE_ERR_INDEX;
    }

    // TODO: Implement seek
    return PPDB_DATABASE_ERR_NOT_FOUND;
}

// Move to next entry
ppdb_error_t ppdb_iterator_next(ppdb_iterator_t* iterator) {
    if (!iterator || !iterator->index) return PPDB_DATABASE_ERR_INDEX;

    if (!iterator->iter) return PPDB_DATABASE_ERR_NOT_FOUND;

    ppdb_error_t err = ppdb_base_skiplist_iterator_next(iterator->iter);
    iterator->valid = (err == PPDB_OK);
    return err;
}

// Move to previous entry
ppdb_error_t ppdb_iterator_prev(ppdb_iterator_t* iterator) {
    if (!iterator || !iterator->index) return PPDB_DATABASE_ERR_INDEX;

    if (!iterator->iter) return PPDB_DATABASE_ERR_NOT_FOUND;

    // TODO: Implement reverse iteration
    return PPDB_DATABASE_ERR_NOT_FOUND;
}

// Check if iterator is valid
bool ppdb_iterator_valid(ppdb_iterator_t* iterator) {
    if (!iterator || !iterator->index) return false;
    return iterator->valid;
}

// Get current key
ppdb_error_t ppdb_iterator_key(ppdb_iterator_t* iterator,
                              void** key, size_t* key_size) {
    if (!iterator || !iterator->index || !key || !key_size) {
        return PPDB_DATABASE_ERR_INDEX;
    }

    if (!iterator->iter || !iterator->valid) {
        return PPDB_DATABASE_ERR_NOT_FOUND;
    }

    return ppdb_base_skiplist_iterator_key(iterator->iter, key, key_size);
}

// Get current value
ppdb_error_t ppdb_iterator_value(ppdb_iterator_t* iterator,
                                void** value, size_t* value_size) {
    if (!iterator || !iterator->index || !value || !value_size) {
        return PPDB_DATABASE_ERR_INDEX;
    }

    if (!iterator->iter || !iterator->valid) {
        return PPDB_DATABASE_ERR_NOT_FOUND;
    }

    return ppdb_base_skiplist_iterator_value(iterator->iter, value, value_size);
} 