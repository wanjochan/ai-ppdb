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

// Test basic data operations
static void test_data_basic_operations(void) {
    printf("  Running test: data_basic_operations\n");
    
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size;
    
    // Put data
    assert(ppdb_storage_put(table, key, strlen(key), value, strlen(value) + 1) == PPDB_OK);
    
    // Get data
    size = sizeof(buffer);
    assert(ppdb_storage_get(table, key, strlen(key), buffer, &size) == PPDB_OK);
    assert(strcmp(buffer, value) == 0);
    
    // Delete data
    assert(ppdb_storage_delete(table, key, strlen(key)) == PPDB_OK);
    
    // Verify deletion
    assert(ppdb_storage_get(table, key, strlen(key), buffer, &size) == PPDB_ERR_NOT_FOUND);
    
    printf("  Test passed: data_basic_operations\n");
}

// Test data operations with invalid parameters
static void test_data_invalid_params(void) {
    printf("  Running test: data_invalid_params\n");
    
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size = sizeof(buffer);
    
    // Test NULL parameters
    assert(ppdb_storage_put(NULL, key, strlen(key), value, strlen(value)) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_put(table, NULL, strlen(key), value, strlen(value)) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_put(table, key, strlen(key), NULL, strlen(value)) == PPDB_ERR_NULL_POINTER);
    
    assert(ppdb_storage_get(NULL, key, strlen(key), buffer, &size) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_get(table, NULL, strlen(key), buffer, &size) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_get(table, key, strlen(key), NULL, &size) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_get(table, key, strlen(key), buffer, NULL) == PPDB_ERR_NULL_POINTER);
    
    assert(ppdb_storage_delete(NULL, key, strlen(key)) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_delete(table, NULL, strlen(key)) == PPDB_ERR_NULL_POINTER);
    
    // Test zero-length parameters
    assert(ppdb_storage_put(table, key, 0, value, strlen(value)) == PPDB_ERR_INVALID_ARGUMENT);
    assert(ppdb_storage_put(table, key, strlen(key), value, 0) == PPDB_ERR_INVALID_ARGUMENT);
    
    assert(ppdb_storage_get(table, key, 0, buffer, &size) == PPDB_ERR_INVALID_ARGUMENT);
    assert(ppdb_storage_delete(table, key, 0) == PPDB_ERR_INVALID_ARGUMENT);
    
    printf("  Test passed: data_invalid_params\n");
}

// Test data operations with large values
static void test_data_large_values(void) {
    printf("  Running test: data_large_values\n");
    
    // Create large key and value
    char large_key[1024];
    char large_value[1024 * 1024];  // 1MB
    char* buffer = malloc(sizeof(large_value));
    size_t size;
    
    memset(large_key, 'K', sizeof(large_key) - 1);
    large_key[sizeof(large_key) - 1] = '\0';
    
    memset(large_value, 'V', sizeof(large_value) - 1);
    large_value[sizeof(large_value) - 1] = '\0';
    
    // Put large data
    assert(ppdb_storage_put(table, large_key, strlen(large_key), 
                           large_value, strlen(large_value) + 1) == PPDB_OK);
    
    // Get large data
    size = sizeof(large_value);
    assert(ppdb_storage_get(table, large_key, strlen(large_key), 
                           buffer, &size) == PPDB_OK);
    assert(strcmp(buffer, large_value) == 0);
    
    // Delete large data
    assert(ppdb_storage_delete(table, large_key, strlen(large_key)) == PPDB_OK);
    
    free(buffer);
    printf("  Test passed: data_large_values\n");
}

// Test multiple data operations
static void test_data_multiple_operations(void) {
    printf("  Running test: data_multiple_operations\n");
    
    const int num_entries = 1000;
    char key[32];
    char value[32];
    char buffer[32];
    size_t size;
    
    // Insert multiple entries
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        assert(ppdb_storage_put(table, key, strlen(key), 
                               value, strlen(value) + 1) == PPDB_OK);
    }
    
    // Read all entries
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        size = sizeof(buffer);
        assert(ppdb_storage_get(table, key, strlen(key), 
                               buffer, &size) == PPDB_OK);
        assert(strcmp(buffer, value) == 0);
    }
    
    // Delete all entries
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        assert(ppdb_storage_delete(table, key, strlen(key)) == PPDB_OK);
    }
    
    // Verify all entries are deleted
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        size = sizeof(buffer);
        assert(ppdb_storage_get(table, key, strlen(key), 
                               buffer, &size) == PPDB_ERR_NOT_FOUND);
    }
    
    printf("  Test passed: data_multiple_operations\n");
}

int main(void) {
    printf("Running test suite: Storage Data Tests\n");
    
    test_setup();
    
    test_data_basic_operations();
    test_data_invalid_params();
    test_data_large_values();
    test_data_multiple_operations();
    
    test_teardown();
    
    printf("Test suite completed\n");
    return 0;
}