#include "ppdb/ppdb.h"
#include <cosmopolitan.h>

// Forward declarations
static uint64_t lemur64(void);
static ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height);
static void node_destroy(ppdb_node_t* node);
static void node_ref(ppdb_node_t* node);
static void node_unref(ppdb_node_t* node);
static uint32_t random_level(void);
static ppdb_error_t init_metrics(ppdb_metrics_t* metrics);
static void cleanup_base(ppdb_base_t* base);
static uint32_t get_shard_index(const ppdb_key_t* key, uint32_t shard_count);

// MurmurHash3 implementation for consistent hashing
static void MurmurHash3_x86_32(const void* key, int len, uint32_t seed, void* out) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;
    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
    
    // Process 4-byte blocks
    for (int i = -nblocks; i; i--) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }
    
    // Process remaining bytes
    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3:
            k1 ^= tail[2] << 16;
            /* fallthrough */
        case 2:
            k1 ^= tail[1] << 8;
            /* fallthrough */
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h1 ^= k1;
    }
    
    // Finalization
    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;
    
    *(uint32_t*)out = h1;
}

// Thread-local random number generator implementation
static pthread_key_t lemur_key;
static pthread_once_t lemur_key_once = PTHREAD_ONCE_INIT;

static void init_lemur_key(void) {
    pthread_key_create(&lemur_key, free);
}

static uint64_t lemur64(void) {
    pthread_once(&lemur_key_once, init_lemur_key);
    
    uint64_t* x = pthread_getspecific(lemur_key);
    if (x == NULL) {
        x = malloc(sizeof(uint64_t));
        *x = (uint64_t)time(NULL) ^ (uint64_t)pthread_self();
        pthread_setspecific(lemur_key, x);
    }
    
    *x += 0x9e3779b97f4a7c15;
    uint64_t z = *x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

// Get node height with atomic access
static uint32_t node_get_height(ppdb_node_t* node) {
    return ppdb_sync_counter_load(&node->height);
}

// Create a new skiplist node with the given key, value and height
static ppdb_node_t* node_create(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value, uint32_t height) {
    if (!base || !key || !value) return NULL;
    
    // Allocate node with variable-length next pointers array
    size_t node_size = sizeof(ppdb_node_t) + height * sizeof(ppdb_node_t*);
    ppdb_node_t* node = PPDB_ALIGNED_ALLOC(node_size);
    if (!node) return NULL;

    // Initialize atomic counters
    ppdb_sync_counter_init(&node->height, height);
    ppdb_sync_counter_init(&node->is_deleted, 0);  // false
    ppdb_sync_counter_init(&node->is_garbage, 0);  // false
    ppdb_sync_counter_init(&node->ref_count, 1);
    
    // Clear next pointers array
    memset(node->next, 0, height * sizeof(ppdb_node_t*));

    // Allocate and initialize key
    node->key = PPDB_ALIGNED_ALLOC(sizeof(ppdb_key_t));
    if (!node->key) goto fail_key;
    node->key->data = PPDB_ALIGNED_ALLOC(key->size);
    if (!node->key->data) goto fail_key_data;
    node->key->size = key->size;
    ppdb_sync_counter_init(&node->key->ref_count, 1);
    memcpy(node->key->data, key->data, key->size);

    // Allocate and initialize value
    node->value = PPDB_ALIGNED_ALLOC(sizeof(ppdb_value_t));
    if (!node->value) goto fail_value;
    node->value->data = PPDB_ALIGNED_ALLOC(value->size);
    if (!node->value->data) goto fail_value_data;
    node->value->size = value->size;
    ppdb_sync_counter_init(&node->value->ref_count, 1);
    memcpy(node->value->data, value->data, value->size);

    // Initialize node lock
    if (ppdb_sync_create(&node->lock, &(ppdb_sync_config_t){
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = base->config.use_lockfree,
        .max_readers = 32,
        .backoff_us = 1,
        .max_retries = 100
    }) != PPDB_OK) goto fail_lock;

    return node;

    // Error handling with cleanup
fail_lock:
    PPDB_ALIGNED_FREE(node->value->data);
fail_value_data:
    PPDB_ALIGNED_FREE(node->value);
fail_value:
    PPDB_ALIGNED_FREE(node->key->data);
fail_key_data:
    PPDB_ALIGNED_FREE(node->key);
fail_key:
    PPDB_ALIGNED_FREE(node);
    return NULL;
}

// Fixed node destruction with proper lock handling
static void node_destroy(ppdb_node_t* node) {
    if (!node) return;
    
    // If write lock is held, destroy immediately
    if (ppdb_sync_try_write_lock(node->lock) == PPDB_OK) {
        ppdb_sync_destroy(node->lock);
        if (node->value) {
            PPDB_ALIGNED_FREE(node->value->data);
            PPDB_ALIGNED_FREE(node->value);
        }
        if (node->key) {
            PPDB_ALIGNED_FREE(node->key->data);
            PPDB_ALIGNED_FREE(node->key);
        }
        PPDB_ALIGNED_FREE(node);
    }
    // Otherwise, mark as deleted and wait for GC
    else {
        ppdb_sync_counter_store(&node->is_deleted, 1);  // true
        ppdb_sync_counter_store(&node->is_garbage, 1);  // true
    }
}

static void node_ref(ppdb_node_t* node) {
    ppdb_sync_counter_add(&node->ref_count, 1);
}

static void node_unref(ppdb_node_t* node) {
    if (ppdb_sync_counter_sub(&node->ref_count, 1) == 1) {
        node_destroy(node);
    }
}

// Generate random level (using thread-local random number generator)
static uint32_t random_level(void) {
    uint32_t level = 1;
    uint32_t rnd = lemur64() & 0xFFFFFFFF;  // Use lemur64 for random number generation
    while ((rnd & 1) && level < MAX_SKIPLIST_LEVEL) {
        level++;
        rnd >>= 1;
    }
    return level;
}

// Helper function to aggregate shard stats for aggregation
static ppdb_error_t aggregate_shard_stats(ppdb_base_t* base, ppdb_metrics_t* stats) {
    if (!base || !stats) return PPDB_ERR_NULL_POINTER;
    if (!base->array.ptrs) return PPDB_ERR_NOT_INITIALIZED;

    // Initialize stats
    ppdb_sync_counter_init(&stats->get_count, 0);
    ppdb_sync_counter_init(&stats->get_hits, 0);
    ppdb_sync_counter_init(&stats->put_count, 0);
    ppdb_sync_counter_init(&stats->remove_count, 0);

    // Iterate over all shards and accumulate stats
    for (uint32_t i = 0; i < base->array.count; i++) {
        ppdb_base_t* shard = base->array.ptrs[i];
        if (!shard) continue;

        ppdb_sync_counter_add(&stats->get_count, 
            ppdb_sync_counter_load(&shard->metrics.get_count));
        ppdb_sync_counter_add(&stats->get_hits, 
            ppdb_sync_counter_load(&shard->metrics.get_hits));
        ppdb_sync_counter_add(&stats->put_count, 
            ppdb_sync_counter_load(&shard->metrics.put_count));
        ppdb_sync_counter_add(&stats->remove_count, 
            ppdb_sync_counter_load(&shard->metrics.remove_count));
    }

    return PPDB_OK;
}

// Unified counter initialization function
static ppdb_error_t init_metrics(ppdb_metrics_t* metrics) {
    if (!metrics) return PPDB_ERR_NULL_POINTER;
    
    ppdb_error_t err = PPDB_OK;
    err = ppdb_sync_counter_init(&metrics->get_count, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->get_hits, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->put_count, 0);
    if (err != PPDB_OK) return err;
    
    err = ppdb_sync_counter_init(&metrics->remove_count, 0);
    if (err != PPDB_OK) return err;
    
    return PPDB_OK;
}

// Unified cleanup function
static void cleanup_base(ppdb_base_t* base) {
    if (!base) return;

    // Get write lock (if exists)
    if (base->storage.lock) {
        ppdb_sync_write_lock(base->storage.lock);
    }

    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE: {
            // Destroy skiplist nodes
            if (base->storage.head) {
                ppdb_node_t* current = base->storage.head;
                while (current) {
                    ppdb_node_t* next = current->next[0];
                    node_unref(current);
                    current = next;
                }
                base->storage.head = NULL;
            }
            break;
        }
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE: {
            // Destroy all shards
            if (base->array.ptrs) {
                for (uint32_t i = 0; i < base->array.count; i++) {
                    if (base->array.ptrs[i]) {
                        ppdb_destroy(base->array.ptrs[i]);
                    }
                }
                PPDB_ALIGNED_FREE(base->array.ptrs);
                base->array.ptrs = NULL;
                base->array.count = 0;
            }
            break;
        }
    }

    // Release write lock and destroy lock
    if (base->storage.lock) {
        ppdb_sync_write_unlock(base->storage.lock);
        ppdb_sync_destroy(base->storage.lock);
        base->storage.lock = NULL;
    }

    // Destroy flush lock (if exists)
    if (base->mem.flush_lock) {
        ppdb_sync_destroy(base->mem.flush_lock);
        base->mem.flush_lock = NULL;
    }

    // Release advanced interface
    if (base->advance) {
        PPDB_ALIGNED_FREE(base->advance);
        base->advance = NULL;
    }

    // Release path
    if (base->path) {
        PPDB_ALIGNED_FREE(base->path);
        base->path = NULL;
    }
}

// Calculate key shard index
static uint32_t get_shard_index(const ppdb_key_t* key, uint32_t shard_count) {
    // Use MurmurHash3 to calculate hash
    uint32_t hash = 0;
    uint32_t seed = 0x12345678;
    MurmurHash3_x86_32(key->data, key->size, seed, &hash);
    return hash % shard_count;
}

// Helper function to validate and setup default configuration
static ppdb_error_t validate_and_setup_config(ppdb_config_t* config) {
    if (!config) return PPDB_ERR_NULL_POINTER;

    // Validate storage type
    switch (config->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE:
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE:
            break;
        default:
            return PPDB_ERR_INVALID_TYPE;
    }

    // Set default values
    if (config->memtable_size == 0) {
        config->memtable_size = DEFAULT_MEMTABLE_SIZE;
    }
    if (config->shard_count == 0) {
        config->shard_count = DEFAULT_SHARD_COUNT;
    }

    // Validate parameter range
    if (config->memtable_size < 1024 || config->memtable_size > (1ULL << 31)) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }
    if (config->shard_count < 1 || config->shard_count > 256) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    return PPDB_OK;
}

// Public interface implementation
ppdb_error_t ppdb_create(ppdb_base_t** base, const ppdb_config_t* config) {
    if (!base || !config) return PPDB_ERR_NULL_POINTER;

    // Validate configuration
    ppdb_config_t validated_config = *config;
    ppdb_error_t err = validate_and_setup_config(&validated_config);
    if (err != PPDB_OK) return err;

    // Allocate base structure
    *base = PPDB_ALIGNED_ALLOC(sizeof(ppdb_base_t));
    if (!*base) return PPDB_ERR_OUT_OF_MEMORY;
    memset(*base, 0, sizeof(ppdb_base_t));

    // Copy configuration
    (*base)->config = validated_config;
    (*base)->type = validated_config.type;

    if (validated_config.path) {
        size_t path_len = strlen(validated_config.path);
        if (path_len >= MAX_PATH_LENGTH) {
            PPDB_ALIGNED_FREE(*base);
            return PPDB_ERR_INVALID_ARGUMENT;
        }
        (*base)->path = PPDB_ALIGNED_ALLOC(path_len + 1);
        if (!(*base)->path) {
            PPDB_ALIGNED_FREE(*base);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        strcpy((*base)->path, validated_config.path);
    }

    // Initialize storage based on type
    switch (validated_config.type) {
        case PPDB_TYPE_SKIPLIST: {
            // Create head node (using max level)
            ppdb_key_t dummy_key = {NULL, 0};
            ppdb_value_t dummy_value = {NULL, 0};
            (*base)->storage.head = node_create(*base, &dummy_key, &dummy_value, MAX_SKIPLIST_LEVEL);
            if (!(*base)->storage.head) {
                err = PPDB_ERR_OUT_OF_MEMORY;
                goto cleanup;
            }

            // Initialize storage level lock
            if (ppdb_sync_create(&(*base)->storage.lock, &(ppdb_sync_config_t){
                .type = PPDB_SYNC_RWLOCK,
                .use_lockfree = (*base)->config.use_lockfree,
                .max_readers = 1024,
                .backoff_us = 1,
                .max_retries = 100
            }) != PPDB_OK) {
                err = PPDB_ERR_LOCK_FAILED;
                goto cleanup;
            }
            break;
        }
        case PPDB_TYPE_MEMTABLE: {
            // Initialize skiplist part
            err = ppdb_create(base, &(ppdb_config_t){
                .type = PPDB_TYPE_SKIPLIST,
                .use_lockfree = validated_config.use_lockfree
            });
            if (err != PPDB_OK) goto cleanup;

            // Set memory limit
            (*base)->mem.limit = validated_config.memtable_size;
            ppdb_sync_counter_init(&(*base)->mem.used, sizeof(ppdb_node_t));
            
            // Initialize flush lock
            if (ppdb_sync_create(&(*base)->mem.flush_lock, &(ppdb_sync_config_t){
                .type = PPDB_SYNC_MUTEX,
                .use_lockfree = false,
                .backoff_us = 1,
                .max_retries = 100
            }) != PPDB_OK) {
                err = PPDB_ERR_LOCK_FAILED;
                goto cleanup;
            }
            break;
        }
        case PPDB_TYPE_SHARDED: {
            // Initialize shard array
            (*base)->array.count = validated_config.shard_count;
            (*base)->array.ptrs = PPDB_ALIGNED_ALLOC((*base)->array.count * sizeof(ppdb_base_t*));
            if (!(*base)->array.ptrs) {
                err = PPDB_ERR_OUT_OF_MEMORY;
                goto cleanup;
            }
            memset((*base)->array.ptrs, 0, (*base)->array.count * sizeof(ppdb_base_t*));

            // Initialize each shard
            for (uint32_t i = 0; i < (*base)->array.count; i++) {
                err = ppdb_create(&(*base)->array.ptrs[i], &(ppdb_config_t){
                    .type = PPDB_TYPE_MEMTABLE,
                    .use_lockfree = validated_config.use_lockfree,
                    .memtable_size = validated_config.memtable_size
                });
                if (err != PPDB_OK) goto cleanup;
            }
            break;
        }
        case PPDB_TYPE_KVSTORE: {
            // KV storage based on shard storage
            err = ppdb_create(base, &(ppdb_config_t){
                .type = PPDB_TYPE_SHARDED,
                .use_lockfree = validated_config.use_lockfree,
                .shard_count = validated_config.shard_count,
                .memtable_size = validated_config.memtable_size,
                .path = validated_config.path
            });
            if (err != PPDB_OK) goto cleanup;
            break;
        }
        default:
            err = PPDB_ERR_INVALID_TYPE;
            goto cleanup;
    }

    // Initialize stats
    err = init_metrics(&(*base)->metrics);
    if (err != PPDB_OK) goto cleanup;

    return PPDB_OK;

cleanup:
    if (*base) {
        cleanup_base(*base);
        PPDB_ALIGNED_FREE(*base);
        *base = NULL;
    }
    return err;
}

void ppdb_destroy(ppdb_base_t* base) {
    if (!base) return;
    cleanup_base(base);
    PPDB_ALIGNED_FREE(base);
}

ppdb_error_t ppdb_put(ppdb_base_t* base, const ppdb_key_t* key, const ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = PPDB_OK;
    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE: {
            // For MEMTABLE type, check memory limit
            if (base->type == PPDB_TYPE_MEMTABLE) {
                // Pre-calculate random level to ensure accurate memory usage statistics
                uint32_t height = random_level();
                size_t node_size = sizeof(ppdb_node_t) + height * sizeof(ppdb_node_t*);
                size_t total_size = node_size + key->size + value->size;

                // Use CAS operation to check and update memory usage
                while (1) {
                    size_t current = ppdb_sync_counter_load(&base->mem.used);
                    if (current + total_size > base->mem.limit) {
                        // Get flush lock
                        if (ppdb_sync_lock(base->mem.flush_lock) != PPDB_OK) {
                            return PPDB_ERR_BUSY;
                        }

                        // Check again inside the lock
                        current = ppdb_sync_counter_load(&base->mem.used);
                        if (current + total_size > base->mem.limit) {
                            err = ppdb_storage_flush(base);
                            if (err != PPDB_OK) {
                                ppdb_sync_unlock(base->mem.flush_lock);
                                return err;
                            }
                        }
                        ppdb_sync_unlock(base->mem.flush_lock);
                        continue;  // Retry memory check
                    }

                    if (ppdb_sync_counter_cas(&base->mem.used, current, current + total_size)) {
                        break;  // Successfully updated
                    }
                }
            }

            // Generate random level
            uint32_t height = random_level();
            ppdb_node_t* new_node = node_create(base, key, value, height);
            if (!new_node) return PPDB_ERR_OUT_OF_MEMORY;

            // Get global write lock
            if ((err = ppdb_sync_write_lock(base->storage.lock)) != PPDB_OK) {
                node_destroy(new_node);
                return err;
            }

            ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
            ppdb_node_t* current = base->storage.head;
            
            // Search from highest level to find insertion position
            for (int level = MAX_SKIPLIST_LEVEL - 1; level >= 0; level--) {
                while (current->next[level]) {
                    ppdb_node_t* next = current->next[level];
                    
                    // Skip deleted nodes
                    if (ppdb_sync_counter_load(&next->is_deleted) || 
                        ppdb_sync_counter_load(&next->is_garbage)) {
                        current = next;
                        continue;
                    }
                    
                    int cmp = memcmp(next->key->data, key->data,
                                MIN(next->key->size, key->size));
                    if (cmp > 0 || (cmp == 0 && next->key->size > key->size)) break;
                    
                    current = next;
                }
                update[level] = current;
            }
            
            // Check if the key already exists
            ppdb_node_t* next = current->next[0];
            if (next && next->key->size == key->size &&
                memcmp(next->key->data, key->data, key->size) == 0) {
                ppdb_sync_write_unlock(base->storage.lock);
                node_destroy(new_node);
                return PPDB_ERR_ALREADY_EXISTS;
            }
            
            // Update pointers
            for (uint32_t i = 0; i < height; i++) {
                new_node->next[i] = update[i]->next[i];
                update[i]->next[i] = new_node;
            }
            
            // Release global lock
            ppdb_sync_write_unlock(base->storage.lock);
            
            ppdb_sync_counter_add(&base->metrics.put_count, 1);
            return PPDB_OK;
        }
        
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE: {
            // Calculate shard index
            uint32_t index = get_shard_index(key, base->array.count);
            ppdb_base_t* shard = base->array.ptrs[index];
            if (!shard) return PPDB_ERR_NOT_INITIALIZED;

            // Insert into corresponding shard
            err = ppdb_put(shard, key, value);
            if (err == PPDB_OK) {
                ppdb_sync_counter_add(&base->metrics.put_count, 1);
            }
            return err;
        }
        
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_remove(ppdb_base_t* base, const ppdb_key_t* key) {
    if (!base || !key) return PPDB_ERR_NULL_POINTER;

    // Inline implementation of skiplist_remove logic
    ppdb_error_t err;
    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE: {
            ppdb_node_t* update[MAX_SKIPLIST_LEVEL];
            ppdb_node_t* current = base->storage.head;

            // Get storage level write lock
            if ((err = ppdb_sync_write_lock(base->storage.lock)) != PPDB_OK) {
                return err;
            }

            // Search from highest level to find
            for (int level = MAX_SKIPLIST_LEVEL - 1; level >= 0; level--) {
                while (current->next[level]) {
                    ppdb_node_t* next = current->next[level];
                    
                    // Get node read lock
                    if (ppdb_sync_read_lock(next->lock) != PPDB_OK) {
                        break;
                    }
                    
                    // Skip deleted nodes
                    if (ppdb_sync_counter_load(&next->is_deleted) || 
                        ppdb_sync_counter_load(&next->is_garbage)) {
                        ppdb_sync_read_unlock(next->lock);
                        current = next;
                        continue;
                    }
                    
                    // Compare key
                    int cmp = memcmp(next->key->data, key->data,
                                MIN(next->key->size, key->size));
                    if (cmp > 0 || (cmp == 0 && next->key->size > key->size)) {
                        ppdb_sync_read_unlock(next->lock);
                        break;
                    }
                    
                    if (cmp == 0 && next->key->size == key->size) {
                        // Found the node to delete
                        ppdb_sync_read_unlock(next->lock);
                        update[level] = current;
                        break;
                    }
                    
                    current = next;
                    ppdb_sync_read_unlock(next->lock);
                }
                update[level] = current;
            }

            // Check if the node to delete is found
            ppdb_node_t* target = update[0]->next[0];
            if (!target || target->key->size != key->size ||
                memcmp(target->key->data, key->data, key->size) != 0) {
                ppdb_sync_write_unlock(base->storage.lock);
                return PPDB_ERR_NOT_FOUND;
            }

            // Get write lock of the target node
            if ((err = ppdb_sync_write_lock(target->lock)) != PPDB_OK) {
                ppdb_sync_write_unlock(base->storage.lock);
                return err;
            }

            // Mark as deleted
            ppdb_sync_counter_store(&target->is_deleted, 1);  // true

            // Update pointers
            for (uint32_t i = 0; i < ppdb_sync_counter_load(&target->height); i++) {
                if (update[i]->next[i] != target) {
                    break;
                }
                update[i]->next[i] = target->next[i];
            }

            // Release lock
            ppdb_sync_write_unlock(target->lock);
            ppdb_sync_write_unlock(base->storage.lock);

            // Increment delete count
            ppdb_sync_counter_add(&base->metrics.remove_count, 1);
            return PPDB_OK;
        }
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE:
            // TODO: Implement delete operation for shard storage
            return PPDB_ERR_NOT_IMPLEMENTED;
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_storage_sync(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE:
            return PPDB_OK;  // Memory storage does not need synchronization
        case PPDB_TYPE_SHARDED:
            // Synchronize all shards
            for (uint32_t i = 0; i < base->array.count; i++) {
                if (base->array.ptrs[i]) {
                    ppdb_error_t err = ppdb_storage_sync(base->array.ptrs[i]);
                    if (err != PPDB_OK) return err;
                }
            }
            return PPDB_OK;
        case PPDB_TYPE_KVSTORE:
            return ppdb_storage_sync(base->array.ptrs[0]);  // Synchronize main storage
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_storage_flush(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = PPDB_OK;
    bool flush_lock_held = false;
    bool storage_lock_held = false;
    ppdb_base_t* new_base = NULL;  // Modify to pointer
    bool new_base_initialized = false;

    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
            return PPDB_OK;  // Base skiplist does not need flushing

        case PPDB_TYPE_MEMTABLE: {
            // Get flush lock
            if ((err = ppdb_sync_lock(base->mem.flush_lock)) != PPDB_OK) {
                goto cleanup;
            }
            flush_lock_held = true;

            // Initialize new skiplist
            err = ppdb_create(&new_base, &(ppdb_config_t){
                .type = PPDB_TYPE_SKIPLIST,
                .use_lockfree = base->config.use_lockfree
            });
            if (err != PPDB_OK) {
                goto cleanup;
            }
            new_base_initialized = true;

            // Get storage level write lock
            if ((err = ppdb_sync_write_lock(base->storage.lock)) != PPDB_OK) {
                goto cleanup;
            }
            storage_lock_held = true;

            // Iterate over original skiplist, write data to disk and build new skiplist
            ppdb_node_t* current = base->storage.head->next[0];
            while (current) {
                // TODO: Write data to disk
                // This needs to implement SSTable write logic
                
                // Migrate valid data to new skiplist
                if (!ppdb_sync_counter_load(&current->is_deleted)) {
                    err = ppdb_put(new_base, current->key, current->value);
                    if (err != PPDB_OK) {
                        goto cleanup;
                    }
                }
                current = current->next[0];
            }

            // Replace old skiplist (under storage level write lock protection)
            ppdb_node_t* old_head = base->storage.head;
            base->storage.head = new_base->storage.head;
            new_base->storage.head = old_head;

            // Clean up old skiplist
            ppdb_node_t* cleanup_node = old_head;
            while (cleanup_node) {
                ppdb_node_t* next = cleanup_node->next[0];
                node_unref(cleanup_node);
                cleanup_node = next;
            }

            // Reset memory usage
            ppdb_sync_counter_store(&base->mem.used, sizeof(ppdb_node_t));
            break;
        }

        case PPDB_TYPE_SHARDED:
            // Flush all shards
            for (uint32_t i = 0; i < base->array.count; i++) {
                if (base->array.ptrs[i]) {
                    err = ppdb_storage_flush(base->array.ptrs[i]);
                    if (err != PPDB_OK) return err;
                }
            }
            return PPDB_OK;

        case PPDB_TYPE_KVSTORE:
            return ppdb_storage_flush(base->array.ptrs[0]);  // Flush main storage

        default:
            return PPDB_ERR_INVALID_TYPE;
    }

cleanup:
    if (storage_lock_held) {
        ppdb_sync_write_unlock(base->storage.lock);
    }
    if (err != PPDB_OK && new_base_initialized) {
        ppdb_destroy(new_base);
    }
    if (flush_lock_held) {
        ppdb_sync_unlock(base->mem.flush_lock);
    }
    return err;
}

ppdb_error_t ppdb_storage_compact(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_NULL_POINTER;

    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
            return PPDB_OK;  // Base skiplist does not need compaction
        case PPDB_TYPE_MEMTABLE:
            return ppdb_storage_flush(base);  // For memtable, compaction is flush
        case PPDB_TYPE_SHARDED:
            // Compact all shards
            for (uint32_t i = 0; i < base->array.count; i++) {
                if (base->array.ptrs[i]) {
                    ppdb_error_t err = ppdb_storage_compact(base->array.ptrs[i]);
                    if (err != PPDB_OK) return err;
                }
            }
            return PPDB_OK;
        case PPDB_TYPE_KVSTORE:
            return ppdb_storage_compact(base->array.ptrs[0]);  // Compact main storage
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_storage_get_stats(ppdb_base_t* base, ppdb_metrics_t* stats) {
    if (!base || !stats) return PPDB_ERR_NULL_POINTER;

    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE: {
            // Directly copy stats
            stats->get_count = base->metrics.get_count;
            stats->get_hits = base->metrics.get_hits;
            stats->put_count = base->metrics.put_count;
            stats->remove_count = base->metrics.remove_count;
            return PPDB_OK;
        }
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE: {
            // Aggregate stats of all shards
            return aggregate_shard_stats(base, stats);
        }
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

ppdb_error_t ppdb_get(ppdb_base_t* base, const ppdb_key_t* key, ppdb_value_t* value) {
    if (!base || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_error_t err = PPDB_OK;
    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE: {
            // Get storage level read lock
            if ((err = ppdb_sync_read_lock(base->storage.lock)) != PPDB_OK) {
                return err;
            }

            ppdb_node_t* current = base->storage.head;
            
            // Search from highest level to find
            for (int level = MAX_SKIPLIST_LEVEL - 1; level >= 0; level--) {
                while (1) {
                    // Atomically read next pointer
                    ppdb_node_t* next = current->next[level];
                    if (!next) break;
                    
                    // Get read lock
                    if (ppdb_sync_read_lock(next->lock) != PPDB_OK) {
                        break;
                    }
                    
                    // Check delete flag
                    if (ppdb_sync_counter_load(&next->is_deleted) || 
                        ppdb_sync_counter_load(&next->is_garbage)) {
                        ppdb_sync_read_unlock(next->lock);
                        current = next;
                        continue;
                    }
                    
                    // Compare key
                    int cmp = memcmp(next->key->data, key->data, 
                                MIN(next->key->size, key->size));
                    
                    if (cmp > 0 || (cmp == 0 && next->key->size > key->size)) {
                        ppdb_sync_read_unlock(next->lock);
                        break;
                    }
                    
                    if (cmp == 0 && next->key->size == key->size) {
                        // Found matching key
                        // Increment reference count
                        node_ref(next);
                        
                        // Allocate value memory
                        value->data = PPDB_ALIGNED_ALLOC(next->value->size);
                        if (!value->data) {
                            node_unref(next);
                            ppdb_sync_read_unlock(next->lock);
                            ppdb_sync_read_unlock(base->storage.lock);
                            return PPDB_ERR_OUT_OF_MEMORY;
                        }

                        // Copy data
                        memcpy(value->data, next->value->data, next->value->size);
                        value->size = next->value->size;
                        ppdb_sync_counter_init(&value->ref_count, 1);
                        
                        // Release locks and references
                        ppdb_sync_read_unlock(next->lock);
                        ppdb_sync_read_unlock(base->storage.lock);
                        node_unref(next);
                        
                        ppdb_sync_counter_add(&base->metrics.get_hits, 1);
                        ppdb_sync_counter_add(&base->metrics.get_count, 1);
                        return PPDB_OK;
                    }
                    
                    current = next;
                    ppdb_sync_read_unlock(next->lock);
                }
            }
            
            // Not found
            ppdb_sync_read_unlock(base->storage.lock);
            ppdb_sync_counter_add(&base->metrics.get_count, 1);
            return PPDB_ERR_NOT_FOUND;
        }
        
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE: {
            // Calculate shard index
            uint32_t index = get_shard_index(key, base->array.count);
            ppdb_base_t* shard = base->array.ptrs[index];
            if (!shard) {
                ppdb_sync_counter_add(&base->metrics.get_count, 1);
                return PPDB_ERR_NOT_FOUND;
            }

            // Search in corresponding shard
            err = ppdb_get(shard, key, value);
            if (err == PPDB_OK) {
                ppdb_sync_counter_add(&base->metrics.get_hits, 1);
            }
            ppdb_sync_counter_add(&base->metrics.get_count, 1);
            return err;
        }
        
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

// Iterator structure definition
typedef struct ppdb_iterator {
    ppdb_base_t* base;
    union {
        struct {  // Skiplist iterator
            ppdb_node_t* current;
        } skiplist;
        struct {  // Shard iterator
            uint32_t current_shard;
            void* shard_iter;
        } sharded;
    };
    bool is_valid;
} ppdb_iterator_t;

ppdb_error_t ppdb_iterator_init(ppdb_base_t* base, void** iter) {
    if (!base || !iter) return PPDB_ERR_NULL_POINTER;

    ppdb_iterator_t* iterator = PPDB_ALIGNED_ALLOC(sizeof(ppdb_iterator_t));
    if (!iterator) return PPDB_ERR_OUT_OF_MEMORY;

    iterator->base = base;
    iterator->is_valid = true;

    switch (base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE: {
            iterator->skiplist.current = base->storage.head;
            break;
        }
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE: {
            iterator->sharded.current_shard = 0;
            iterator->sharded.shard_iter = NULL;
            // Find the first valid shard
            while (iterator->sharded.current_shard < base->array.count) {
                if (base->array.ptrs[iterator->sharded.current_shard]) {
                    ppdb_error_t err = ppdb_iterator_init(
                        base->array.ptrs[iterator->sharded.current_shard],
                        &iterator->sharded.shard_iter);
                    if (err != PPDB_OK) {
                        PPDB_ALIGNED_FREE(iterator);
                        return err;
                    }
                    break;
                }
                iterator->sharded.current_shard++;
            }
            // If no valid shard is found, mark as invalid
            if (iterator->sharded.current_shard >= base->array.count) {
                iterator->is_valid = false;
            }
            break;
        }
        default:
            PPDB_ALIGNED_FREE(iterator);
            return PPDB_ERR_INVALID_TYPE;
    }

    *iter = iterator;
    return PPDB_OK;
}

ppdb_error_t ppdb_iterator_next(void* iter, ppdb_key_t* key, ppdb_value_t* value) {
    if (!iter || !key || !value) return PPDB_ERR_NULL_POINTER;

    ppdb_iterator_t* iterator = (ppdb_iterator_t*)iter;
    if (!iterator->is_valid) return PPDB_ERR_ITERATOR_INVALID;

    ppdb_error_t err;
    switch (iterator->base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE: {
            // Get storage level read lock
            if ((err = ppdb_sync_read_lock(iterator->base->storage.lock)) != PPDB_OK) {
                return err;
            }

            // Skip deleted nodes
            while (iterator->skiplist.current->next[0]) {
                ppdb_node_t* next = iterator->skiplist.current->next[0];
                if (ppdb_sync_read_lock(next->lock) != PPDB_OK) {
                    ppdb_sync_read_unlock(iterator->base->storage.lock);
                    return PPDB_ERR_BUSY;
                }

                if (!ppdb_sync_counter_load(&next->is_deleted) && 
                    !ppdb_sync_counter_load(&next->is_garbage)) {
                    // Copy key and value
                    key->data = PPDB_ALIGNED_ALLOC(next->key->size);
                    if (!key->data) {
                        ppdb_sync_read_unlock(next->lock);
                        ppdb_sync_read_unlock(iterator->base->storage.lock);
                        return PPDB_ERR_OUT_OF_MEMORY;
                    }
                    memcpy(key->data, next->key->data, next->key->size);
                    key->size = next->key->size;
                    ppdb_sync_counter_init(&key->ref_count, 1);

                    value->data = PPDB_ALIGNED_ALLOC(next->value->size);
                    if (!value->data) {
                        PPDB_ALIGNED_FREE(key->data);
                        ppdb_sync_read_unlock(next->lock);
                        ppdb_sync_read_unlock(iterator->base->storage.lock);
                        return PPDB_ERR_OUT_OF_MEMORY;
                    }
                    memcpy(value->data, next->value->data, next->value->size);
                    value->size = next->value->size;
                    ppdb_sync_counter_init(&value->ref_count, 1);

                    iterator->skiplist.current = next;
                    ppdb_sync_read_unlock(next->lock);
                    ppdb_sync_read_unlock(iterator->base->storage.lock);
                    return PPDB_OK;
                }

                iterator->skiplist.current = next;
                ppdb_sync_read_unlock(next->lock);
            }

            // Reached end
            iterator->is_valid = false;
            ppdb_sync_read_unlock(iterator->base->storage.lock);
            return PPDB_ERR_ITERATOR_END;
        }
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE: {
            while (iterator->sharded.current_shard < iterator->base->array.count) {
                if (iterator->sharded.shard_iter) {
                    err = ppdb_iterator_next(iterator->sharded.shard_iter, key, value);
                    if (err == PPDB_OK) {
                        return PPDB_OK;
                    }
                    if (err != PPDB_ERR_ITERATOR_END) {
                        return err;
                    }
                    // Current shard traversal completed, release iterator
                    ppdb_iterator_destroy(iterator->sharded.shard_iter);
                    iterator->sharded.shard_iter = NULL;
                }

                // Find next valid shard
                iterator->sharded.current_shard++;
                while (iterator->sharded.current_shard < iterator->base->array.count) {
                    if (iterator->base->array.ptrs[iterator->sharded.current_shard]) {
                        err = ppdb_iterator_init(
                            iterator->base->array.ptrs[iterator->sharded.current_shard],
                            &iterator->sharded.shard_iter);
                        if (err != PPDB_OK) {
                            return err;
                        }
                        // Recursive call to get next element
                        return ppdb_iterator_next(iter, key, value);
                    }
                    iterator->sharded.current_shard++;
                }
            }

            // All shards traversed
            iterator->is_valid = false;
            return PPDB_ERR_ITERATOR_END;
        }
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

void ppdb_iterator_destroy(void* iter) {
    if (!iter) return;
    ppdb_iterator_t* iterator = (ppdb_iterator_t*)iter;

    switch (iterator->base->type) {
        case PPDB_TYPE_SKIPLIST:
        case PPDB_TYPE_MEMTABLE:
            break;  // No special cleanup needed
        case PPDB_TYPE_SHARDED:
        case PPDB_TYPE_KVSTORE:
            if (iterator->sharded.shard_iter) {
                ppdb_iterator_destroy(iterator->sharded.shard_iter);
            }
            break;
    }

    PPDB_ALIGNED_FREE(iterator);
}
