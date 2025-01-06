#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "test_common.h"

// Test storage initialization and cleanup
void test_storage_init(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));

    // Initialize storage layer
    ppdb_storage_config_t storage_config = {
        .memtable_size = PPDB_DEFAULT_MEMTABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    ASSERT_OK(ppdb_storage_init(&storage, base, &storage_config));

    // Verify storage configuration
    ppdb_storage_config_t config;
    ASSERT_OK(ppdb_storage_get_config(storage, &config));
    ASSERT_EQ(config.memtable_size, storage_config.memtable_size);
    ASSERT_EQ(config.block_size, storage_config.block_size);
    ASSERT_EQ(config.cache_size, storage_config.cache_size);
    ASSERT_EQ(config.write_buffer_size, storage_config.write_buffer_size);
    ASSERT_STR_EQ(config.data_dir, storage_config.data_dir);
    ASSERT_EQ(config.use_compression, storage_config.use_compression);
    ASSERT_EQ(config.sync_writes, storage_config.sync_writes);

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

// Test table operations
void test_storage_table(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));

    // Initialize storage layer
    ppdb_storage_config_t storage_config = {
        .memtable_size = PPDB_DEFAULT_MEMTABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    ASSERT_OK(ppdb_storage_init(&storage, base, &storage_config));

    // Create table
    ASSERT_OK(ppdb_table_create(storage, "test_table"));

    // Try to create same table again
    ASSERT_EQ(ppdb_table_create(storage, "test_table"), PPDB_ERR_TABLE_EXISTS);

    // Drop table
    ASSERT_OK(ppdb_table_drop(storage, "test_table"));

    // Try to drop non-existent table
    ASSERT_EQ(ppdb_table_drop(storage, "non_existent"), PPDB_ERR_TABLE_NOT_FOUND);

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

// Test data operations
void test_storage_data(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;
    ppdb_storage_table_t* table = NULL;

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));

    // Initialize storage layer
    ppdb_storage_config_t storage_config = {
        .memtable_size = PPDB_DEFAULT_MEMTABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    ASSERT_OK(ppdb_storage_init(&storage, base, &storage_config));

    // Create table
    ASSERT_OK(ppdb_table_create(storage, "test_table"));

    // Get table
    ASSERT_OK(ppdb_storage_get_table(storage, "test_table", &table));
    ASSERT_NOT_NULL(table);

    // Test data operations
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size;

    // Put data
    ASSERT_OK(ppdb_storage_put(table, key, strlen(key), value, strlen(value)));

    // Get data
    size = sizeof(buffer);
    ASSERT_OK(ppdb_storage_get(table, key, strlen(key), buffer, &size));
    ASSERT_EQ(size, strlen(value));
    ASSERT_STR_EQ(buffer, value);

    // Delete data
    ASSERT_OK(ppdb_storage_delete(table, key, strlen(key)));

    // Try to get deleted data
    size = sizeof(buffer);
    ASSERT_EQ(ppdb_storage_get(table, key, strlen(key), buffer, &size), PPDB_ERR_NOT_FOUND);

    // Drop table
    ASSERT_OK(ppdb_table_drop(storage, "test_table"));

    // Cleanup
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

int main(void) {
    RUN_TEST(test_storage_init);
    RUN_TEST(test_storage_table);
    RUN_TEST(test_storage_data);
    return 0;
} 