#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "test_common.h"

// Test setup and teardown
static ppdb_base_t* base = NULL;
static ppdb_storage_t* storage = NULL;
static ppdb_base_config_t base_config;
static ppdb_storage_config_t storage_config;

static void test_setup(void) {
    // Initialize base config
    base_config = (ppdb_base_config_t){
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    
    // Initialize storage config
    storage_config = (ppdb_storage_config_t){
        .memtable_size = 64 * 1024,      // 64KB
        .block_size = 4096,              // 4KB
        .cache_size = 256 * 1024,        // 256KB
        .write_buffer_size = 64 * 1024,  // 64KB
        .data_dir = "data",
        .use_compression = true,
        .sync_writes = true
    };
    
    // Initialize base layer
    assert(ppdb_base_init(&base, &base_config) == PPDB_OK);
}

static void test_teardown(void) {
    if (storage) {
        ppdb_storage_destroy(storage);
        storage = NULL;
    }
    if (base) {
        ppdb_base_destroy(base);
        base = NULL;
    }
}

// Test initialization
static void test_storage_init_normal(void) {
    printf("  Running test: storage_init_normal\n");
    
    // Test normal initialization
    assert(ppdb_storage_init(&storage, base, &storage_config) == PPDB_OK);
    assert(storage != NULL);
    
    // Cleanup
    ppdb_storage_destroy(storage);
    storage = NULL;
    
    printf("  Test passed: storage_init_normal\n");
}

// Test initialization with invalid parameters
static void test_storage_init_invalid(void) {
    printf("  Running test: storage_init_invalid\n");
    
    // Test NULL parameters
    assert(ppdb_storage_init(NULL, base, &storage_config) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_init(&storage, NULL, &storage_config) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_init(&storage, base, NULL) == PPDB_ERR_NULL_POINTER);
    
    // Test invalid config values
    ppdb_storage_config_t invalid_config = storage_config;
    invalid_config.memtable_size = 0;
    assert(ppdb_storage_init(&storage, base, &invalid_config) == PPDB_ERR_INVALID_ARGUMENT);
    
    invalid_config = storage_config;
    invalid_config.block_size = 0;
    assert(ppdb_storage_init(&storage, base, &invalid_config) == PPDB_ERR_INVALID_ARGUMENT);
    
    invalid_config = storage_config;
    invalid_config.data_dir = NULL;
    assert(ppdb_storage_init(&storage, base, &invalid_config) == PPDB_ERR_INVALID_ARGUMENT);
    
    printf("  Test passed: storage_init_invalid\n");
}

// Test configuration management
static void test_storage_config_management(void) {
    printf("  Running test: storage_config_management\n");
    
    // Initialize storage
    assert(ppdb_storage_init(&storage, base, &storage_config) == PPDB_OK);
    
    // Test get config
    ppdb_storage_config_t current_config;
    assert(ppdb_storage_get_config(storage, &current_config) == PPDB_OK);
    assert(current_config.memtable_size == storage_config.memtable_size);
    assert(current_config.block_size == storage_config.block_size);
    assert(current_config.cache_size == storage_config.cache_size);
    assert(current_config.write_buffer_size == storage_config.write_buffer_size);
    assert(strcmp(current_config.data_dir, storage_config.data_dir) == 0);
    assert(current_config.use_compression == storage_config.use_compression);
    assert(current_config.sync_writes == storage_config.sync_writes);
    
    // Test update config
    ppdb_storage_config_t new_config = current_config;
    new_config.cache_size *= 2;
    new_config.write_buffer_size *= 2;
    new_config.use_compression = !current_config.use_compression;
    new_config.sync_writes = !current_config.sync_writes;
    
    assert(ppdb_storage_update_config(storage, &new_config) == PPDB_OK);
    
    // Verify updated config
    assert(ppdb_storage_get_config(storage, &current_config) == PPDB_OK);
    assert(current_config.cache_size == new_config.cache_size);
    assert(current_config.write_buffer_size == new_config.write_buffer_size);
    assert(current_config.use_compression == new_config.use_compression);
    assert(current_config.sync_writes == new_config.sync_writes);
    
    // Cleanup
    ppdb_storage_destroy(storage);
    storage = NULL;
    
    printf("  Test passed: storage_config_management\n");
}

// Test statistics
static void test_storage_statistics(void) {
    printf("  Running test: storage_statistics\n");
    
    // Initialize storage
    assert(ppdb_storage_init(&storage, base, &storage_config) == PPDB_OK);
    
    // Get initial stats
    ppdb_storage_stats_t stats;
    ppdb_storage_get_stats(storage, &stats);
    assert(ppdb_base_counter_get(stats.reads) == 0);
    assert(ppdb_base_counter_get(stats.writes) == 0);
    assert(ppdb_base_counter_get(stats.flushes) == 0);
    assert(ppdb_base_counter_get(stats.compactions) == 0);
    assert(ppdb_base_counter_get(stats.cache_hits) == 0);
    assert(ppdb_base_counter_get(stats.cache_misses) == 0);
    assert(ppdb_base_counter_get(stats.wal_syncs) == 0);
    
    // Cleanup
    ppdb_storage_destroy(storage);
    storage = NULL;
    
    printf("  Test passed: storage_statistics\n");
}

int main(void) {
    printf("Running test suite: Storage Initialization Tests\n");
    
    test_setup();
    
    test_storage_init_normal();
    test_storage_init_invalid();
    test_storage_config_management();
    test_storage_statistics();
    
    test_teardown();
    
    printf("Test suite completed\n");
    return 0;
}