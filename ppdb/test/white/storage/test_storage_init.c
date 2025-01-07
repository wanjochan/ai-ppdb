#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "../test_framework.h"

// Test setup and teardown
static ppdb_base_t* base = NULL;
static ppdb_storage_t* storage = NULL;
static ppdb_base_config_t base_config;
static ppdb_storage_config_t storage_config;

static int test_setup(void) {
    printf("Setting up test environment...\n");
    
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
    ppdb_error_t err = ppdb_base_init(&base, &base_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(base);
    
    return 0;
}

static int test_teardown(void) {
    if (storage) {
        ppdb_storage_destroy(storage);
        storage = NULL;
    }
    if (base) {
        ppdb_base_destroy(base);
        base = NULL;
    }
    return 0;
}

// Test initialization
static int test_storage_init_normal(void) {
    printf("  Running test: storage_init_normal\n");
    
    // Test normal initialization
    ppdb_error_t err = ppdb_storage_init(&storage, base, &storage_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(storage);
    
    // Cleanup
    ppdb_storage_destroy(storage);
    storage = NULL;
    
    printf("  Test passed: storage_init_normal\n");
    return 0;
}

// Test initialization with invalid parameters
static int test_storage_init_invalid(void) {
    printf("  Running test: storage_init_invalid\n");
    
    // Test NULL parameters
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_init(NULL, base, &storage_config));
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_init(&storage, NULL, &storage_config));
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_init(&storage, base, NULL));
    
    // Test invalid config values
    ppdb_storage_config_t invalid_config = storage_config;
    invalid_config.memtable_size = 0;
    TEST_ASSERT_EQUALS(PPDB_ERR_INVALID_ARGUMENT, ppdb_storage_init(&storage, base, &invalid_config));
    
    invalid_config = storage_config;
    invalid_config.block_size = 0;
    TEST_ASSERT_EQUALS(PPDB_ERR_INVALID_ARGUMENT, ppdb_storage_init(&storage, base, &invalid_config));
    
    invalid_config = storage_config;
    invalid_config.data_dir = NULL;
    TEST_ASSERT_EQUALS(PPDB_ERR_INVALID_ARGUMENT, ppdb_storage_init(&storage, base, &invalid_config));
    
    printf("  Test passed: storage_init_invalid\n");
    return 0;
}

// Test configuration management
static int test_storage_config_management(void) {
    printf("  Running test: storage_config_management\n");
    
    // Initialize storage
    ppdb_error_t err = ppdb_storage_init(&storage, base, &storage_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(storage);
    
    // Test get config
    ppdb_storage_config_t current_config = {0};
    err = ppdb_storage_get_config(storage, &current_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(storage_config.memtable_size, current_config.memtable_size);
    TEST_ASSERT_EQUALS(storage_config.block_size, current_config.block_size);
    TEST_ASSERT_EQUALS(storage_config.cache_size, current_config.cache_size);
    TEST_ASSERT_EQUALS(storage_config.write_buffer_size, current_config.write_buffer_size);
    TEST_ASSERT_EQUALS(0, strcmp(current_config.data_dir, storage_config.data_dir));
    TEST_ASSERT_EQUALS(storage_config.use_compression, current_config.use_compression);
    TEST_ASSERT_EQUALS(storage_config.sync_writes, current_config.sync_writes);
    
    // Test update config
    ppdb_storage_config_t new_config = current_config;
    new_config.cache_size *= 2;
    new_config.write_buffer_size *= 2;
    new_config.use_compression = !current_config.use_compression;
    new_config.sync_writes = !current_config.sync_writes;
    
    err = ppdb_storage_update_config(storage, &new_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    // Verify updated config
    err = ppdb_storage_get_config(storage, &current_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(new_config.cache_size, current_config.cache_size);
    TEST_ASSERT_EQUALS(new_config.write_buffer_size, current_config.write_buffer_size);
    TEST_ASSERT_EQUALS(new_config.use_compression, current_config.use_compression);
    TEST_ASSERT_EQUALS(new_config.sync_writes, current_config.sync_writes);
    
    // Cleanup
    ppdb_storage_destroy(storage);
    storage = NULL;
    
    printf("  Test passed: storage_config_management\n");
    return 0;
}

// Test statistics
static int test_storage_statistics(void) {
    printf("  Running test: storage_statistics\n");
    
    // Initialize storage
    ppdb_error_t err = ppdb_storage_init(&storage, base, &storage_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(storage);
    
    // Get initial stats
    ppdb_storage_stats_t stats;
    err = ppdb_storage_get_stats(storage, &stats);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(0, ppdb_base_counter_get(stats.reads));
    TEST_ASSERT_EQUALS(0, ppdb_base_counter_get(stats.writes));
    TEST_ASSERT_EQUALS(0, ppdb_base_counter_get(stats.flushes));
    TEST_ASSERT_EQUALS(0, ppdb_base_counter_get(stats.compactions));
    TEST_ASSERT_EQUALS(0, ppdb_base_counter_get(stats.cache_hits));
    TEST_ASSERT_EQUALS(0, ppdb_base_counter_get(stats.cache_misses));
    TEST_ASSERT_EQUALS(0, ppdb_base_counter_get(stats.wal_syncs));
    
    // Cleanup
    ppdb_storage_destroy(storage);
    storage = NULL;
    
    printf("  Test passed: storage_statistics\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    test_setup();
    
    TEST_RUN(test_storage_init_normal);
    TEST_RUN(test_storage_init_invalid);
    TEST_RUN(test_storage_config_management);
    TEST_RUN(test_storage_statistics);
    
    test_teardown();
    
    TEST_CLEANUP();
    return 0;
}