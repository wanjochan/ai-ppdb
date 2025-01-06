#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "test_common.h"

// Test setup and teardown
static ppdb_base_t* base = NULL;
static ppdb_storage_t* storage = NULL;
static ppdb_storage_table_t* table = NULL;
static ppdb_base_config_t base_config;
static ppdb_storage_config_t storage_config;

static void test_setup(void) {
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
    assert(ppdb_base_init(&base, &base_config) == PPDB_OK);
    assert(ppdb_storage_init(&storage, base, &storage_config) == PPDB_OK);
    assert(ppdb_storage_create_table(storage, "test_table", &table) == PPDB_OK);
}

static void test_teardown(void) {
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
}

// Test flush operations
static void test_flush_operations(void) {
    printf("  Running test: flush_operations\n");
    
    // Insert some data
    const int num_entries = 1000;
    char key[32];
    char value[32];
    
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        assert(ppdb_storage_put(table, key, strlen(key), 
                               value, strlen(value) + 1) == PPDB_OK);
    }
    
    // Flush memtable
    assert(ppdb_storage_flush(table) == PPDB_OK);
    
    // Verify data after flush
    char buffer[32];
    size_t size;
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        size = sizeof(buffer);
        assert(ppdb_storage_get(table, key, strlen(key), 
                               buffer, &size) == PPDB_OK);
        assert(strcmp(buffer, value) == 0);
    }
    
    printf("  Test passed: flush_operations\n");
}

// Test compaction operations
static void test_compaction_operations(void) {
    printf("  Running test: compaction_operations\n");
    
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
            assert(ppdb_storage_put(table, key, strlen(key), 
                                   value, strlen(value) + 1) == PPDB_OK);
        }
        
        // Flush after each batch
        assert(ppdb_storage_flush(table) == PPDB_OK);
    }
    
    // Trigger compaction
    assert(ppdb_storage_compact(table) == PPDB_OK);
    
    // Verify all data after compaction
    char buffer[32];
    size_t size;
    for (int batch = 0; batch < num_batches; batch++) {
        for (int i = 0; i < entries_per_batch; i++) {
            snprintf(key, sizeof(key), "key_%d_%d", batch, i);
            snprintf(value, sizeof(value), "value_%d_%d", batch, i);
            size = sizeof(buffer);
            assert(ppdb_storage_get(table, key, strlen(key), 
                                   buffer, &size) == PPDB_OK);
            assert(strcmp(buffer, value) == 0);
        }
    }
    
    printf("  Test passed: compaction_operations\n");
}

// Test backup and restore operations
static void test_backup_restore_operations(void) {
    printf("  Running test: backup_restore_operations\n");
    
    // Insert some data
    const int num_entries = 1000;
    char key[32];
    char value[32];
    
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        assert(ppdb_storage_put(table, key, strlen(key), 
                               value, strlen(value) + 1) == PPDB_OK);
    }
    
    // Create backup
    assert(ppdb_storage_backup(table, "backup_test") == PPDB_OK);
    
    // Delete all data
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        assert(ppdb_storage_delete(table, key, strlen(key)) == PPDB_OK);
    }
    
    // Restore from backup
    assert(ppdb_storage_restore(table, "backup_test") == PPDB_OK);
    
    // Verify restored data
    char buffer[32];
    size_t size;
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        size = sizeof(buffer);
        assert(ppdb_storage_get(table, key, strlen(key), 
                               buffer, &size) == PPDB_OK);
        assert(strcmp(buffer, value) == 0);
    }
    
    printf("  Test passed: backup_restore_operations\n");
}

// Test maintenance operations with invalid parameters
static void test_maintain_invalid_params(void) {
    printf("  Running test: maintain_invalid_params\n");
    
    // Test NULL parameters
    assert(ppdb_storage_flush(NULL) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_compact(NULL) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_backup(NULL, "backup") == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_backup(table, NULL) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_restore(NULL, "backup") == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_restore(table, NULL) == PPDB_ERR_NULL_POINTER);
    
    // Test invalid backup names
    assert(ppdb_storage_backup(table, "") == PPDB_ERR_INVALID_ARGUMENT);
    assert(ppdb_storage_backup(table, "   ") == PPDB_ERR_INVALID_ARGUMENT);
    assert(ppdb_storage_restore(table, "") == PPDB_ERR_INVALID_ARGUMENT);
    assert(ppdb_storage_restore(table, "   ") == PPDB_ERR_INVALID_ARGUMENT);
    
    printf("  Test passed: maintain_invalid_params\n");
}

int main(void) {
    printf("Running test suite: Storage Maintenance Tests\n");
    
    test_setup();
    
    test_flush_operations();
    test_compaction_operations();
    test_backup_restore_operations();
    test_maintain_invalid_params();
    
    test_teardown();
    
    printf("Test suite completed\n");
    return 0;
}