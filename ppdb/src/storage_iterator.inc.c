// Iterator implementation
// This file is included by storage.c

// Iterator structure definition
typedef struct ppdb_iterator {
    ppdb_base_t* base;
    union {
        struct {
            ppdb_node_t* current;
            uint32_t shard_index;
        } mem;
        // Reserved for future iterator types
        void* reserved;
    } data;
} ppdb_iterator_t;

ppdb_error_t ppdb_iterator_init(ppdb_base_t* base, void** iter) {
    if (!base || !iter) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Allocate iterator
    ppdb_iterator_t* it = PPDB_ALIGNED_ALLOC(sizeof(ppdb_iterator_t));
    if (!it) {
        return PPDB_ERR_NO_MEMORY;
    }
    memset(it, 0, sizeof(ppdb_iterator_t));

    // Initialize iterator
    it->base = base;
    it->data.mem.shard_index = 0;
    
    // Find first non-empty shard
    while (it->data.mem.shard_index < base->config.shard_count) {
        ppdb_shard_t* shard = &base->shards[it->data.mem.shard_index];
        if (shard->head && shard->head->next[0]) {
            it->data.mem.current = shard->head->next[0];
            node_ref(it->data.mem.current);
            break;
        }
        it->data.mem.shard_index++;
    }

    *iter = it;
    return PPDB_OK;
}

ppdb_error_t ppdb_iterator_next(void* iter, ppdb_key_t* key, ppdb_value_t* value) {
    if (!iter || !key || !value) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    ppdb_iterator_t* iterator = (ppdb_iterator_t*)iter;
    ppdb_base_t* base = iterator->base;

    // Check if we've reached the end
    if (iterator->data.mem.shard_index >= base->config.shard_count) {
        return PPDB_ERR_NOT_FOUND;
    }

    // Get current node
    ppdb_node_t* current = iterator->data.mem.current;
    if (!current) {
        return PPDB_ERR_NOT_FOUND;
    }

    // Copy key and value
    if (current && !ppdb_sync_counter_get(&current->is_deleted)) {
        key->size = current->key->size;
        key->data = PPDB_ALIGNED_ALLOC(key->size);
        if (!key->data) {
            return PPDB_ERR_NO_MEMORY;
        }
        memcpy(key->data, current->key->data, key->size);

        value->size = current->value->size;
        value->data = PPDB_ALIGNED_ALLOC(value->size);
        if (!value->data) {
            PPDB_ALIGNED_FREE(key->data);
            return PPDB_ERR_NO_MEMORY;
        }
        memcpy(value->data, current->value->data, value->size);

        return PPDB_OK;
    }

    // Move to next node or shard
    ppdb_node_t* next = current->next[0];
    if (next) {
        node_ref(next);
        node_unref(current);
        iterator->data.mem.current = next;
    } else {
        // Move to next non-empty shard
        node_unref(current);
        iterator->data.mem.current = NULL;
        iterator->data.mem.shard_index++;
        
        while (iterator->data.mem.shard_index < base->config.shard_count) {
            ppdb_shard_t* shard = &base->shards[iterator->data.mem.shard_index];
            if (shard->head && shard->head->next[0]) {
                iterator->data.mem.current = shard->head->next[0];
                node_ref(iterator->data.mem.current);
                break;
            }
            iterator->data.mem.shard_index++;
        }
    }

    return ppdb_iterator_next(iter, key, value);
}

void ppdb_iterator_destroy(void* iter) {
    if (!iter) {
        return;
    }

    ppdb_iterator_t* it = (ppdb_iterator_t*)iter;
    
    // Release current node if any
    if (it->data.mem.current) {
        node_unref(it->data.mem.current);
    }

    // Free iterator
    PPDB_ALIGNED_FREE(it);
}
