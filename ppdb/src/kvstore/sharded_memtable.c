#include <cosmopolitan.h>
#include "internal/sharded_memtable.h"
#include "internal/sync.h"
#include "ppdb/logger.h"

// Calculate key hash
static uint32_t hash_key(const uint8_t* key, size_t key_len) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < key_len; i++) {
        hash = ((hash << 5) + hash) + key[i];
    }
    return hash;
}

// Calculate shard index based on key
static uint32_t get_shard_index(const sharded_memtable_t* table,
                              const uint8_t* key, size_t key_len) {
    uint32_t hash = hash_key(key, key_len);
    return hash & ((1 << table->config.shard_bits) - 1);
}

// Sharded memtable structure
struct sharded_memtable_t {
    shard_config_t config;     // Shard configuration
    atomic_skiplist_t** shards;  // Shard array
    ppdb_sync_t* sync;         // Global synchronization
    ppdb_sync_t** shard_syncs; // Shard synchronization
    atomic_size_t total_size;         // Total element count
};

// Create sharded memtable
sharded_memtable_t* sharded_memtable_create(const shard_config_t* config) {
    sharded_memtable_t* table = (sharded_memtable_t*)malloc(sizeof(sharded_memtable_t));
    if (!table) {
        ppdb_log_error("Failed to allocate sharded memtable");
        return NULL;
    }

    // Copy configuration
    memcpy(&table->config, config, sizeof(shard_config_t));

    // Allocate shard array
    table->shards = (atomic_skiplist_t**)malloc(config->shard_count * sizeof(atomic_skiplist_t*));
    if (!table->shards) {
        free(table);
        ppdb_log_error("Failed to allocate shards array");
        return NULL;
    }

    // Initialize each shard
    for (uint32_t i = 0; i < config->shard_count; i++) {
        table->shards[i] = atomic_skiplist_create();
        if (!table->shards[i]) {
            for (uint32_t j = 0; j < i; j++) {
                atomic_skiplist_destroy(table->shards[j]);
            }
            free(table->shards);
            free(table);
            ppdb_log_error("Failed to create skiplist for shard %u", i);
            return NULL;
        }
    }

    // Initialize global synchronization
    table->sync = ppdb_sync_create();
    if (!table->sync) {
        for (uint32_t i = 0; i < config->shard_count; i++) {
            atomic_skiplist_destroy(table->shards[i]);
        }
        free(table->shards);
        free(table);
        ppdb_log_error("Failed to create global sync");
        return NULL;
    }

    // Initialize shard synchronization
    table->shard_syncs = (ppdb_sync_t**)malloc(config->shard_count * sizeof(ppdb_sync_t*));
    if (!table->shard_syncs) {
        ppdb_sync_destroy(table->sync);
        for (uint32_t i = 0; i < config->shard_count; i++) {
            atomic_skiplist_destroy(table->shards[i]);
        }
        free(table->shards);
        free(table);
        ppdb_log_error("Failed to allocate shard syncs array");
        return NULL;
    }
    for (uint32_t i = 0; i < config->shard_count; i++) {
        table->shard_syncs[i] = ppdb_sync_create();
        if (!table->shard_syncs[i]) {
            ppdb_sync_destroy(table->sync);
            for (uint32_t j = 0; j < i; j++) {
                ppdb_sync_destroy(table->shard_syncs[j]);
            }
            free(table->shard_syncs);
            for (uint32_t j = 0; j < config->shard_count; j++) {
                atomic_skiplist_destroy(table->shards[j]);
            }
            free(table->shards);
            free(table);
            ppdb_log_error("Failed to create shard sync for shard %u", i);
            return NULL;
        }
    }

    atomic_init(&table->total_size, 0);
    return table;
}

// Destroy sharded memtable
void sharded_memtable_destroy(sharded_memtable_t* table) {
    if (!table) return;

    // Destroy all shards
    for (uint32_t i = 0; i < table->config.shard_count; i++) {
        atomic_skiplist_destroy(table->shards[i]);
    }

    // Destroy shard synchronization
    for (uint32_t i = 0; i < table->config.shard_count; i++) {
        ppdb_sync_destroy(table->shard_syncs[i]);
    }
    free(table->shard_syncs);

    // Destroy global synchronization
    ppdb_sync_destroy(table->sync);

    free(table->shards);
    free(table);
}

// Insert key-value pair
int sharded_memtable_put(sharded_memtable_t* table, const uint8_t* key, size_t key_len,
                        const uint8_t* value, size_t value_len) {
    if (!table || !key || !value) {
        return -1;
    }

    // Get global lock
    ppdb_sync_lock(table->sync);

    uint32_t shard_index = get_shard_index(table, key, key_len);
    atomic_skiplist_t* shard = table->shards[shard_index];

    // Get shard lock
    ppdb_sync_lock(table->shard_syncs[shard_index]);

    // Check if shard size exceeds limit
    if (atomic_skiplist_size(shard) >= table->config.max_size) {
        ppdb_sync_unlock(table->shard_syncs[shard_index]);
        ppdb_sync_unlock(table->sync);
        ppdb_log_error("Shard %u is full", shard_index);
        return -2;
    }

    int result = atomic_skiplist_put(shard, key, key_len, value, value_len);
    if (result == 0) {
        atomic_fetch_add_explicit(&table->total_size, 1, memory_order_relaxed);
    }

    // Release shard lock
    ppdb_sync_unlock(table->shard_syncs[shard_index]);

    // Release global lock
    ppdb_sync_unlock(table->sync);

    return result;
}

// Delete key-value pair
int sharded_memtable_delete(sharded_memtable_t* table, const uint8_t* key, size_t key_len) {
    if (!table || !key) {
        return -1;
    }

    // Get global lock
    ppdb_sync_lock(table->sync);

    uint32_t shard_index = get_shard_index(table, key, key_len);
    atomic_skiplist_t* shard = table->shards[shard_index];

    // Get shard lock
    ppdb_sync_lock(table->shard_syncs[shard_index]);

    int result = atomic_skiplist_delete(shard, key, key_len);
    if (result == 0) {
        atomic_fetch_sub_explicit(&table->total_size, 1, memory_order_relaxed);
    }

    // Release shard lock
    ppdb_sync_unlock(table->shard_syncs[shard_index]);

    // Release global lock
    ppdb_sync_unlock(table->sync);

    return result;
}

// Find key-value pair
int sharded_memtable_get(sharded_memtable_t* table, const uint8_t* key, size_t key_len,
                        uint8_t* value, size_t* value_len) {
    if (!table || !key || !value || !value_len) {
        return -1;
    }

    // Get global lock
    ppdb_sync_lock(table->sync);

    uint32_t shard_index = get_shard_index(table, key, key_len);
    atomic_skiplist_t* shard = table->shards[shard_index];

    // Get shard lock
    ppdb_sync_lock(table->shard_syncs[shard_index]);

    int result = atomic_skiplist_get(shard, key, key_len, value, value_len);

    // Release shard lock
    ppdb_sync_unlock(table->shard_syncs[shard_index]);

    // Release global lock
    ppdb_sync_unlock(table->sync);

    return result;
}

// Get total element count
size_t sharded_memtable_size(sharded_memtable_t* table) {
    if (!table) return 0;

    // Get global lock
    ppdb_sync_lock(table->sync);

    size_t size = atomic_load_explicit(&table->total_size, memory_order_relaxed);

    // Release global lock
    ppdb_sync_unlock(table->sync);

    return size;
}

// Get element count of specified shard
size_t sharded_memtable_shard_size(sharded_memtable_t* table, uint32_t shard_index) {
    if (!table || shard_index >= table->config.shard_count) return 0;

    // Get global lock
    ppdb_sync_lock(table->sync);

    size_t size = atomic_skiplist_size(table->shards[shard_index]);

    // Release global lock
    ppdb_sync_unlock(table->sync);

    return size;
}

// Clear sharded memtable
void sharded_memtable_clear(sharded_memtable_t* table) {
    if (!table) return;

    // Get global lock
    ppdb_sync_lock(table->sync);

    for (uint32_t i = 0; i < table->config.shard_count; i++) {
        // Get shard lock
        ppdb_sync_lock(table->shard_syncs[i]);

        atomic_skiplist_clear(table->shards[i]);

        // Release shard lock
        ppdb_sync_unlock(table->shard_syncs[i]);
    }

    atomic_store_explicit(&table->total_size, 0, memory_order_relaxed);

    // Release global lock
    ppdb_sync_unlock(table->sync);
}

// Iterate through sharded memtable
void sharded_memtable_foreach(sharded_memtable_t* table, memtable_visitor_t visitor, void* ctx) {
    if (!table || !visitor) return;

    // Get global lock
    ppdb_sync_lock(table->sync);

    for (uint32_t i = 0; i < table->config.shard_count; i++) {
        // Get shard lock
        ppdb_sync_lock(table->shard_syncs[i]);

        atomic_skiplist_foreach(table->shards[i], (skiplist_visitor_t)visitor, ctx);

        // Release shard lock
        ppdb_sync_unlock(table->shard_syncs[i]);
    }

    // Release global lock
    ppdb_sync_unlock(table->sync);
}
