// Node Operations
//

ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height) {
    if (!base || height == 0 || height > PPDB_MAX_HEIGHT) {
        return NULL;
    }

    size_t node_size = sizeof(ppdb_node_t) + height * sizeof(ppdb_node_t*);
    ppdb_node_t* node = PPDB_ALIGNED_ALLOC(node_size);
    if (!node) {
        return NULL;
    }

    // 初始化内存
    memset(node, 0, node_size);
    
    // 初始化计数器
    ppdb_error_t err;
    err = ppdb_sync_counter_init(&node->height, height);
    if (err != PPDB_OK) {
        PPDB_ALIGNED_FREE(node);
        return NULL;
    }
    
    err = ppdb_sync_counter_init(&node->ref_count, 1);
    if (err != PPDB_OK) {
        ppdb_sync_counter_cleanup(&node->height);
        PPDB_ALIGNED_FREE(node);
        return NULL;
    }
    
    err = ppdb_sync_counter_init(&node->is_deleted, 0);
    if (err != PPDB_OK) {
        ppdb_sync_counter_cleanup(&node->ref_count);
        ppdb_sync_counter_cleanup(&node->height);
        PPDB_ALIGNED_FREE(node);
        return NULL;
    }
    
    err = ppdb_sync_counter_init(&node->is_garbage, 0);
    if (err != PPDB_OK) {
        ppdb_sync_counter_cleanup(&node->is_deleted);
        ppdb_sync_counter_cleanup(&node->ref_count);
        ppdb_sync_counter_cleanup(&node->height);
        PPDB_ALIGNED_FREE(node);
        return NULL;
    }
    
    // 初始化锁
    ppdb_sync_config_t lock_config = {
        .type = PPDB_SYNC_SPINLOCK,
        .use_lockfree = base->config.use_lockfree,
        .enable_ref_count = true,
        .max_readers = 32,
        .backoff_us = 1,
        .max_retries = 100
    };
    
    err = ppdb_sync_create(&node->lock, &lock_config);
    if (err != PPDB_OK) {
        ppdb_sync_counter_cleanup(&node->is_garbage);
        ppdb_sync_counter_cleanup(&node->is_deleted);
        ppdb_sync_counter_cleanup(&node->ref_count);
        ppdb_sync_counter_cleanup(&node->height);
        PPDB_ALIGNED_FREE(node);
        return NULL;
    }
    
    // 复制键值（如果提供）
    if (key) {
        node->key = PPDB_ALIGNED_ALLOC(sizeof(ppdb_key_t));
        if (!node->key) {
            ppdb_sync_destroy(node->lock);
            ppdb_sync_counter_cleanup(&node->is_garbage);
            ppdb_sync_counter_cleanup(&node->is_deleted);
            ppdb_sync_counter_cleanup(&node->ref_count);
            ppdb_sync_counter_cleanup(&node->height);
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        memset(node->key, 0, sizeof(ppdb_key_t));
        err = ppdb_sync_counter_init(&node->key->ref_count, 1);
        if (err != PPDB_OK) {
            PPDB_ALIGNED_FREE(node->key);
            ppdb_sync_destroy(node->lock);
            ppdb_sync_counter_cleanup(&node->is_garbage);
            ppdb_sync_counter_cleanup(&node->is_deleted);
            ppdb_sync_counter_cleanup(&node->ref_count);
            ppdb_sync_counter_cleanup(&node->height);
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        
        if (ppdb_key_copy(node->key, key) != PPDB_OK) {
            ppdb_sync_counter_cleanup(&node->key->ref_count);
            PPDB_ALIGNED_FREE(node->key);
            ppdb_sync_destroy(node->lock);
            ppdb_sync_counter_cleanup(&node->is_garbage);
            ppdb_sync_counter_cleanup(&node->is_deleted);
            ppdb_sync_counter_cleanup(&node->ref_count);
            ppdb_sync_counter_cleanup(&node->height);
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
    }
    
    if (value) {
        node->value = PPDB_ALIGNED_ALLOC(sizeof(ppdb_value_t));
        if (!node->value) {
            if (node->key) {
                ppdb_sync_counter_cleanup(&node->key->ref_count);
                PPDB_ALIGNED_FREE(node->key);
            }
            ppdb_sync_destroy(node->lock);
            ppdb_sync_counter_cleanup(&node->is_garbage);
            ppdb_sync_counter_cleanup(&node->is_deleted);
            ppdb_sync_counter_cleanup(&node->ref_count);
            ppdb_sync_counter_cleanup(&node->height);
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        memset(node->value, 0, sizeof(ppdb_value_t));
        err = ppdb_sync_counter_init(&node->value->ref_count, 1);
        if (err != PPDB_OK) {
            PPDB_ALIGNED_FREE(node->value);
            if (node->key) {
                ppdb_sync_counter_cleanup(&node->key->ref_count);
                PPDB_ALIGNED_FREE(node->key);
            }
            ppdb_sync_destroy(node->lock);
            ppdb_sync_counter_cleanup(&node->is_garbage);
            ppdb_sync_counter_cleanup(&node->is_deleted);
            ppdb_sync_counter_cleanup(&node->ref_count);
            ppdb_sync_counter_cleanup(&node->height);
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
        
        if (ppdb_value_copy(node->value, value) != PPDB_OK) {
            ppdb_sync_counter_cleanup(&node->value->ref_count);
            PPDB_ALIGNED_FREE(node->value);
            if (node->key) {
                ppdb_sync_counter_cleanup(&node->key->ref_count);
                PPDB_ALIGNED_FREE(node->key);
            }
            ppdb_sync_destroy(node->lock);
            ppdb_sync_counter_cleanup(&node->is_garbage);
            ppdb_sync_counter_cleanup(&node->is_deleted);
            ppdb_sync_counter_cleanup(&node->ref_count);
            ppdb_sync_counter_cleanup(&node->height);
            PPDB_ALIGNED_FREE(node);
            return NULL;
        }
    }
    
    return node;
}

void node_destroy(ppdb_node_t* node) {
    if (!node) {
        return;
    }

    if (node->key) {
        ppdb_key_cleanup(node->key);
        PPDB_ALIGNED_FREE(node->key);
    }

    if (node->value) {
        ppdb_value_cleanup(node->value);
        PPDB_ALIGNED_FREE(node->value);
    }

    if (node->lock) {
        ppdb_sync_destroy(node->lock);
        PPDB_ALIGNED_FREE(node->lock);
    }

    PPDB_ALIGNED_FREE(node);
}

uint32_t node_get_height(ppdb_node_t* node) {
    if (!node) {
        return 0;
    }
    return ppdb_sync_counter_load(&node->height);
}

void node_ref(ppdb_node_t* node) {
    if (!node) {
        return;
    }
    ppdb_sync_counter_add(&node->ref_count, 1);
}

void node_unref(ppdb_node_t* node) {
    if (!node) {
        return;
    }
    if (ppdb_sync_counter_sub(&node->ref_count, 1) == 0) {
        node_destroy(node);
    }
}

ppdb_node_t* node_get_next(ppdb_node_t* node, uint32_t level) {
    if (!node || level >= node_get_height(node)) {
        return NULL;
    }
    return node->next[level];
}

void node_set_next(ppdb_node_t* node, uint32_t level, ppdb_node_t* next) {
    if (!node || level >= node_get_height(node)) {
        return;
    }
    node->next[level] = next;
}

bool node_cas_next(ppdb_node_t* node, uint32_t level, ppdb_node_t* expected, ppdb_node_t* desired) {
    if (!node || level >= node_get_height(node)) {
        return false;
    }
    ppdb_sync_lock(node->lock);
    bool success = (node->next[level] == expected);
    if (success) {
        node->next[level] = desired;
    }
    ppdb_sync_unlock(node->lock);
    return success;
}

bool node_is_deleted(ppdb_node_t* node) {
    if (!node) {
        return false;
    }
    return ppdb_sync_counter_load(&node->is_deleted) != 0;
}

bool node_mark_deleted(ppdb_node_t* node) {
    if (!node) {
        return false;
    }
    return ppdb_sync_counter_cas(&node->is_deleted, 0, 1);
}

//
// Storage Operations
//

ppdb_error_t ppdb_storage_sync(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Currently a no-op in memkv phase
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_flush(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Lock all shards
    for (uint32_t i = 0; i < base->config.shard_count; i++) {
        ppdb_error_t err = ppdb_sync_write_lock(base->shards[i].lock);
        if (err != PPDB_OK) {
            // Unlock previously locked shards
            for (uint32_t j = 0; j < i; j++) {
                ppdb_sync_write_unlock(base->shards[j].lock);
            }
            return err;
        }
    }

    // Perform flush operations (no-op in memkv phase)

    // Unlock all shards
    for (uint32_t i = 0; i < base->config.shard_count; i++) {
        ppdb_sync_write_unlock(base->shards[i].lock);
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_storage_compact(ppdb_base_t* base) {
    if (!base) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Currently a no-op in memkv phase
    return PPDB_OK;
}

ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_metrics_t* stats) {
    if (!base || !stats) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    // Reset stats
    ppdb_error_t err = init_metrics(stats);
    if (err != PPDB_OK) {
        return err;
    }

    // Aggregate stats from all shards
    ppdb_stats_t temp_stats;
    err = aggregate_shard_stats(base, &temp_stats);
    if (err != PPDB_OK) {
        return err;
    }

    // Copy stats using atomic operations
    ppdb_sync_counter_store(&stats->total_nodes, temp_stats.node_count);
    ppdb_sync_counter_store(&stats->total_keys, temp_stats.key_count);
    ppdb_sync_counter_store(&stats->total_bytes, temp_stats.memory_usage);
    ppdb_sync_counter_store(&stats->total_gets, temp_stats.get_ops);
    ppdb_sync_counter_store(&stats->total_puts, temp_stats.put_ops);
    ppdb_sync_counter_store(&stats->total_removes, temp_stats.remove_ops);

    return PPDB_OK;
}

ppdb_error_t aggregate_shard_stats(ppdb_base_t* base, ppdb_stats_t* stats) {
    if (!base || !stats) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    memset(stats, 0, sizeof(ppdb_stats_t));

    for (uint32_t i = 0; i < base->config.shard_count; i++) {
        ppdb_shard_t* shard = &base->shards[i];
        ppdb_metrics_t* metrics = &shard->metrics;

        stats->node_count += ppdb_sync_counter_get(&metrics->total_nodes);
        stats->key_count += ppdb_sync_counter_get(&metrics->total_keys);
        stats->memory_usage += ppdb_sync_counter_get(&metrics->total_bytes);
        stats->get_ops += ppdb_sync_counter_get(&metrics->total_gets);
        stats->put_ops += ppdb_sync_counter_get(&metrics->total_puts);
        stats->remove_ops += ppdb_sync_counter_get(&metrics->total_removes);
    }

    return PPDB_OK;
}

ppdb_shard_t* get_shard(ppdb_base_t* base, const ppdb_key_t* key) {
    if (!base || !key || !key->data) {
        return NULL;
    }

    uint32_t hash = 0;
    for (size_t i = 0; i < key->size; i++) {
        hash = hash * 31 + ((uint8_t*)key->data)[i];
    }

    uint32_t shard_index = hash % base->config.shard_count;
    return &base->shards[shard_index];
}

static ppdb_random_state_t random_state;

void init_random(void) {
    // 使用当前时间作为种子
    uint64_t seed = (uint64_t)time(NULL);
    ppdb_random_init(&random_state, seed);
}

uint32_t random_level(void) {
    uint32_t level = 1;
    while (level < PPDB_MAX_HEIGHT && ppdb_random_double(&random_state) < PPDB_LEVEL_PROBABILITY) {
        level++;
    }
    return level;
}
