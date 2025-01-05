#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "test_common.h"

static void test_storage_init(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ppdb_storage_config_t storage_config = {
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
    assert(base != NULL);

    // Initialize storage layer
    assert(ppdb_storage_init(&storage, base, &storage_config) == PPDB_OK);
    assert(storage != NULL);

    // Check error cases
    assert(ppdb_storage_init(NULL, base, &storage_config) == PPDB_BASE_ERR_PARAM);
    assert(ppdb_storage_init(&storage, NULL, &storage_config) == PPDB_BASE_ERR_PARAM);
    assert(ppdb_storage_init(&storage, base, NULL) == PPDB_BASE_ERR_PARAM);

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
    ppdb_base_destroy(base);
}

static void test_storage_table(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;
    ppdb_storage_table_t* table = NULL;
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ppdb_storage_config_t storage_config = {
        .memtable_size = 64 * 1024,
        .block_size = 4096,
        .cache_size = 256 * 1024,
        .write_buffer_size = 64 * 1024,
        .data_dir = "data",
        .use_compression = true,
        .sync_writes = true
    };

    // Initialize
    assert(ppdb_base_init(&base, &base_config) == PPDB_OK);
    assert(ppdb_storage_init(&storage, base, &storage_config) == PPDB_OK);

    // Create table
    assert(ppdb_storage_create_table(storage, "test_table", &table) == PPDB_OK);
    assert(table != NULL);

    // Get table
    ppdb_storage_table_t* table2 = NULL;
    assert(ppdb_storage_get_table(storage, "test_table", &table2) == PPDB_OK);
    assert(table2 == table);

    // Drop table
    assert(ppdb_storage_drop_table(storage, "test_table") == PPDB_OK);
    assert(ppdb_storage_get_table(storage, "test_table", &table2) == PPDB_ERR_NOT_FOUND);

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

static void test_storage_data(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;
    ppdb_storage_table_t* table = NULL;
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ppdb_storage_config_t storage_config = {
        .memtable_size = 64 * 1024,
        .block_size = 4096,
        .cache_size = 256 * 1024,
        .write_buffer_size = 64 * 1024,
        .data_dir = "data",
        .use_compression = true,
        .sync_writes = true
    };

    // Initialize
    assert(ppdb_base_init(&base, &base_config) == PPDB_OK);
    assert(ppdb_storage_init(&storage, base, &storage_config) == PPDB_OK);
    assert(ppdb_storage_create_table(storage, "test_table", &table) == PPDB_OK);

    // Test data operations
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size;

    // Put
    assert(ppdb_storage_put(table, key, strlen(key), value, strlen(value) + 1) == PPDB_OK);

    // Get
    size = sizeof(buffer);
    assert(ppdb_storage_get(table, key, strlen(key), buffer, &size) == PPDB_OK);
    assert(strcmp(buffer, value) == 0);

    // Delete
    assert(ppdb_storage_delete(table, key, strlen(key)) == PPDB_OK);
    assert(ppdb_storage_get(table, key, strlen(key), buffer, &size) == PPDB_ERR_NOT_FOUND);

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

int main(void) {
    printf("Running test suite: Storage Tests\n");
    
    printf("  Running test: test_storage_init\n");
    test_storage_init();
    printf("  Test passed: test_storage_init\n");
    
    printf("  Running test: test_storage_table\n");
    test_storage_table();
    printf("  Test passed: test_storage_table\n");
    
    printf("  Running test: test_storage_data\n");
    test_storage_data();
    printf("  Test passed: test_storage_data\n");
    
    printf("Test suite completed\n");
    return 0;
} 