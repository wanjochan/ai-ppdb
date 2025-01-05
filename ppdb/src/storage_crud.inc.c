// Basic CRUD operations
// This file is included by storage.c

ppdb_error_t ppdb_create(ppdb_config_t* config, ppdb_t** out_base) {
    ppdb_error_t err;
    
    // 验证配置
    err = validate_and_setup_config(config);
    if (err != PPDB_OK) {
        return err;
    }
    
    // 初始化随机数生成器
    init_random();
    
    if (!out_base) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Allocate base structure
    ppdb_base_t* b = PPDB_ALIGNED_ALLOC(sizeof(ppdb_base_t));
    if (!b) {
        return PPDB_ERR_NO_MEMORY;
    }
    memset(b, 0, sizeof(ppdb_base_t));

    // Copy configuration
    memcpy(&b->config, config, sizeof(ppdb_config_t));

    // Allocate shards
    b->shards = PPDB_ALIGNED_ALLOC(sizeof(ppdb_shard_t) * config->shard_count);
    if (!b->shards) {
        cleanup_base(b);
        return PPDB_ERR_NO_MEMORY;
    }
    memset(b->shards, 0, sizeof(ppdb_shard_t) * config->shard_count);

    // Initialize each shard
    for (uint32_t i = 0; i < config->shard_count; i++) {
        ppdb_shard_t* shard = &b->shards[i];

        // Initialize metrics
        err = init_metrics(&shard->metrics);
        if (err != PPDB_OK) {
            cleanup_base(b);
            return err;
        }

        // Create head node
        shard->head = node_create(b, NULL, NULL, MAX_SKIPLIST_LEVEL);
        if (!shard->head) {
            cleanup_base(b);
            return PPDB_ERR_NO_MEMORY;
        }

        // Initialize shard lock
        err = ppdb_sync_create(&shard->lock, &(ppdb_sync_config_t){
            .type = PPDB_SYNC_RWLOCK,
            .use_lockfree = config->use_lockfree,
            .max_readers = 32,
            .backoff_us = 1,
            .max_retries = 100
        });
        if (err != PPDB_OK) {
            cleanup_base(b);
            return err;
        }
    }

    *out_base = b;
    return PPDB_OK;
}

void ppdb_destroy(ppdb_base_t* base) {
    cleanup_base(base);
}

ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Get shard
    ppdb_shard_t* shard = get_shard(base, key);
    if (!shard) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Lock shard
    ppdb_error_t err = ppdb_sync_write_lock(shard->lock);
    if (err != PPDB_OK) {
        return err;
    }

    // Find insert position
    ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
    ppdb_node_t* current = shard->head;
    
    for (int i = node_get_height(current) - 1; i >= 0; i--) {
        while (current->next[i] != NULL &&
               memcmp(current->next[i]->key->data, key->data, MIN(current->next[i]->key->size, key->size)) < 0) {
            current = current->next[i];
        }
        update[i] = current;
        i--;
    }
    current = current->next[0];

    // Check if key exists
    if (current && current->key &&
        current->key->size == key->size &&
        memcmp(current->key->data, key->data, key->size) == 0) {
        // Update existing node
        ppdb_value_t* old_value = current->value;
        current->value = PPDB_ALIGNED_ALLOC(sizeof(ppdb_value_t));
        if (!current->value) {
            PPDB_ALIGNED_FREE(old_value);
            current->value = old_value;
            ppdb_sync_write_unlock(shard->lock);
            return PPDB_ERR_NO_MEMORY;
        }
        
        memset(current->value, 0, sizeof(ppdb_value_t));
        current->value->size = value->size;
        current->value->data = PPDB_ALIGNED_ALLOC(value->size);
        if (!current->value->data) {
            PPDB_ALIGNED_FREE(current->value);
            current->value = old_value;
            ppdb_sync_write_unlock(shard->lock);
            return PPDB_ERR_NO_MEMORY;
        }

        memcpy(current->value->data, value->data, value->size);
        ppdb_sync_counter_init(&current->value->ref_count, 1);
        
        // Free old value
        if (old_value->data) {
            PPDB_ALIGNED_FREE(old_value->data);
        }
        PPDB_ALIGNED_FREE(old_value);
    } else {
        // Insert new node
        uint32_t level = random_level();
        ppdb_node_t* node = node_create(base, key, value, level);
        if (!node) {
            ppdb_sync_write_unlock(shard->lock);
            return PPDB_ERR_NO_MEMORY;
        }

        // Update pointers
        for (uint32_t i = 0; i < level; i++) {
            node->next[i] = update[i]->next[i];
            update[i]->next[i] = node;
        }

        // Update metrics
        ppdb_sync_counter_inc(&shard->metrics.total_nodes);
        ppdb_sync_counter_inc(&shard->metrics.total_keys);
        ppdb_sync_counter_add(&shard->metrics.total_bytes, key->size + value->size);
    }

    ppdb_sync_counter_inc(&shard->metrics.total_puts);
    ppdb_sync_write_unlock(shard->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Get shard
    ppdb_shard_t* shard = get_shard(base, key);
    if (!shard) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Lock shard
    ppdb_error_t err = ppdb_sync_read_lock(shard->lock);
    if (err != PPDB_OK) {
        return err;
    }

    // Find key
    ppdb_node_t* current = shard->head;
    for (int i = node_get_height(current) - 1; i >= 0; i--) {
        while (current->next[i] != NULL &&
               memcmp(current->next[i]->key->data, key->data, MIN(current->next[i]->key->size, key->size)) < 0) {
            current = current->next[i];
        }
        i--;
    }
    current = current->next[0];

    // Check if key exists
    if (current && current->key &&
        current->key->size == key->size &&
        memcmp(current->key->data, key->data, key->size) == 0) {
        // Copy value
        value->size = current->value->size;
        value->data = PPDB_ALIGNED_ALLOC(value->size);
        if (!value->data) {
            ppdb_sync_read_unlock(shard->lock);
            return PPDB_ERR_NO_MEMORY;
        }
        memcpy(value->data, current->value->data, value->size);
        ppdb_sync_counter_inc(&shard->metrics.total_gets);
        ppdb_sync_read_unlock(shard->lock);
        return PPDB_OK;
    }

    ppdb_sync_read_unlock(shard->lock);
    return PPDB_ERR_NOT_FOUND;
}

ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    if (!base || !key) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Get shard
    ppdb_shard_t* shard = get_shard(base, key);
    if (!shard) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Lock shard
    ppdb_error_t err = ppdb_sync_write_lock(shard->lock);
    if (err != PPDB_OK) {
        return err;
    }

    // Find remove position
    ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
    ppdb_node_t* current = shard->head;
    
    for (int i = node_get_height(current) - 1; i >= 0; i--) {
        while (current->next[i] != NULL &&
               memcmp(current->next[i]->key->data, key->data, MIN(current->next[i]->key->size, key->size)) < 0) {
            current = current->next[i];
        }
        update[i] = current;
        i--;
    }
    current = current->next[0];

    // Check if key exists
    if (current && current->key &&
        current->key->size == key->size &&
        memcmp(current->key->data, key->data, key->size) == 0) {
        // Update pointers
        for (uint32_t i = 0; i < node_get_height(current); i++) {
            if (update[i]->next[i] != current) {
                break;
            }
            update[i]->next[i] = current->next[i];
        }

        // Update metrics
        ppdb_sync_counter_dec(&shard->metrics.total_nodes);
        ppdb_sync_counter_dec(&shard->metrics.total_keys);
        ppdb_sync_counter_sub(&shard->metrics.total_bytes, 
                            current->key->size + current->value->size);
        ppdb_sync_counter_inc(&shard->metrics.total_removes);

        // Free node
        node_unref(current);
        
        ppdb_sync_write_unlock(shard->lock);
        return PPDB_OK;
    }

    ppdb_sync_write_unlock(shard->lock);
    return PPDB_ERR_NOT_FOUND;
}
