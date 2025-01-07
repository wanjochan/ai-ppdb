#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "../test_framework.h"

// Test setup and teardown
static ppdb_base_t* base = NULL;
static ppdb_storage_t* storage = NULL;
static ppdb_storage_table_t* table = NULL;
static ppdb_base_config_t base_config;
static ppdb_storage_config_t storage_config;

static int test_setup(void) {
    printf("Setting up test environment...\n");
    
    // Initialize base config
    base_config = (ppdb_base_config_t){
        .memory_limit = 1024 * 1024,
        .thread_pool_size = 4,
        .thread_safe = true
    };
    
    // Initialize storage config
    storage_config = (ppdb_storage_config_t){
        .memtable_size = 64 * 1024,
        .block_size = 4096,
        .cache_size = 256 * 1024,
        .write_buffer_size = 64 * 1024,
        .data_dir = "data",
        .use_compression = true,
        .sync_writes = true
    };
    
    // Initialize layers
    ppdb_error_t err = ppdb_base_init(&base, &base_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(base);
    
    err = ppdb_storage_init(&storage, base, &storage_config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(storage);
    
    err = ppdb_storage_create_table(storage, "test_table", &table);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(table);
    
    return 0;
}

static int test_teardown(void) {
    if (table) {
        // Table will be destroyed by storage
        table = NULL;
    }
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

// Test flush operations
static int test_flush_operations(void) {
    printf("  Running test: flush_operations\n");
    
    ppdb_error_t err;
    
    // Insert some data
    const int num_entries = 1000;
    char key[32];
    char value[32];
    
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        err = ppdb_storage_put(table, key, strlen(key), 
                              value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }
    
    // Flush memtable
    err = ppdb_storage_flush(table);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    // Verify data after flush
    char buffer[32];
    size_t size;
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        size = sizeof(buffer);
        err = ppdb_storage_get(table, key, strlen(key), 
                              buffer, &size);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        TEST_ASSERT_EQUALS(0, strcmp(buffer, value));
    }
    
    printf("  Test passed: flush_operations\n");
    return 0;
}

// Test compaction operations
static int test_compaction_operations(void) {
    printf("  Running test: compaction_operations\n");
    
    ppdb_error_t err;
    
    // Insert data in multiple batches to create multiple SSTable files
    const int num_batches = 5;
    const int entries_per_batch = 1000;
    char key[32];
    char value[32];
    
    for (int batch = 0; batch < num_batches; batch++) {
        // Insert batch of data
        for (int i = 0; i < entries_per_batch; i++) {
            snprintf(key, sizeof(key), "key_%d_%d", batch, i);
            snprintf(value, sizeof(value), "value_%d_%d", batch, i);
            err = ppdb_storage_put(table, key, strlen(key), 
                                  value, strlen(value) + 1);
            TEST_ASSERT_EQUALS(PPDB_OK, err);
        }
        
        // Flush after each batch
        err = ppdb_storage_flush(table);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }
    
    // Trigger compaction
    err = ppdb_storage_compact(table);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    // Verify all data after compaction
    char buffer[32];
    size_t size;
    for (int batch = 0; batch < num_batches; batch++) {
        for (int i = 0; i < entries_per_batch; i++) {
            snprintf(key, sizeof(key), "key_%d_%d", batch, i);
            snprintf(value, sizeof(value), "value_%d_%d", batch, i);
            size = sizeof(buffer);
            err = ppdb_storage_get(table, key, strlen(key), 
                                  buffer, &size);
            TEST_ASSERT_EQUALS(PPDB_OK, err);
            TEST_ASSERT_EQUALS(0, strcmp(buffer, value));
        }
    }
    
    printf("  Test passed: compaction_operations\n");
    return 0;
}

// Test backup and restore operations
static int test_backup_restore_operations(void) {
    printf("  Running test: backup_restore_operations\n");
    
    ppdb_error_t err;
    
    // Insert some data
    const int num_entries = 1000;
    char key[32];
    char value[32];
    
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        err = ppdb_storage_put(table, key, strlen(key), 
                              value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }
    
    // Create backup
    err = ppdb_storage_backup(table, "backup_test");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    // Delete all data
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        err = ppdb_storage_delete(table, key, strlen(key));
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }
    
    // Restore from backup
    err = ppdb_storage_restore(table, "backup_test");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    // Verify restored data
    char buffer[32];
    size_t size;
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        size = sizeof(buffer);
        err = ppdb_storage_get(table, key, strlen(key), 
                              buffer, &size);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        TEST_ASSERT_EQUALS(0, strcmp(buffer, value));
    }
    
    printf("  Test passed: backup_restore_operations\n");
    return 0;
}

// Test maintenance operations with invalid parameters
static int test_maintain_invalid_params(void) {
    printf("  Running test: maintain_invalid_params\n");
    
    // Test NULL parameters
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_flush(NULL));
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_compact(NULL));
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_backup(NULL, "backup"));
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_backup(table, NULL));
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_restore(NULL, "backup"));
    TEST_ASSERT_EQUALS(PPDB_ERR_NULL_POINTER, ppdb_storage_restore(table, NULL));
    
    // Test invalid backup names
    TEST_ASSERT_EQUALS(PPDB_ERR_INVALID_ARGUMENT, ppdb_storage_backup(table, ""));
    TEST_ASSERT_EQUALS(PPDB_ERR_INVALID_ARGUMENT, ppdb_storage_backup(table, "   "));
    TEST_ASSERT_EQUALS(PPDB_ERR_INVALID_ARGUMENT, ppdb_storage_restore(table, ""));
    TEST_ASSERT_EQUALS(PPDB_ERR_INVALID_ARGUMENT, ppdb_storage_restore(table, "   "));
    
    printf("  Test passed: maintain_invalid_params\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    test_setup();
    
    TEST_RUN(test_flush_operations);
    TEST_RUN(test_compaction_operations);
    TEST_RUN(test_backup_restore_operations);
    TEST_RUN(test_maintain_invalid_params);
    
    test_teardown();
    
    TEST_CLEANUP();
    return 0;
}