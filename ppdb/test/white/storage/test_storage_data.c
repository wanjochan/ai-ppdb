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

// Test basic data operations
static int test_data_basic_operations(void) {
    printf("  Running test: data_basic_operations\n");
    
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size = sizeof(buffer);
    ppdb_error_t err;
    
    // Put data
    err = ppdb_storage_put(table, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    // Get data
    err = ppdb_storage_get(table, key, strlen(key), buffer, &size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(0, strcmp(buffer, value));
    
    // Delete data
    err = ppdb_storage_delete(table, key, strlen(key));
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    // Verify deletion
    size = sizeof(buffer);
    err = ppdb_storage_get(table, key, strlen(key), buffer, &size);
    TEST_ASSERT_EQUALS(PPDB_ERR_NOT_FOUND, err);
    
    printf("  Test passed: data_basic_operations\n");
    return 0;
}

// Test data operations with invalid parameters
static int test_data_invalid_params(void) {
    printf("  Running test: data_invalid_params\n");
    
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size = sizeof(buffer);
    
    // Test NULL parameters
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_put(NULL, key, strlen(key), value, strlen(value)));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_put(table, NULL, strlen(key), value, strlen(value)));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_put(table, key, strlen(key), NULL, strlen(value)));
    
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_get(NULL, key, strlen(key), buffer, &size));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_get(table, NULL, strlen(key), buffer, &size));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_get(table, key, strlen(key), NULL, &size));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_get(table, key, strlen(key), buffer, NULL));
    
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_delete(NULL, key, strlen(key)));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_delete(table, NULL, strlen(key)));
    
    // Test zero-length parameters
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_put(table, key, 0, value, strlen(value)));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_put(table, key, strlen(key), value, 0));
    
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_get(table, key, 0, buffer, &size));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_delete(table, key, 0));
    
    printf("  Test passed: data_invalid_params\n");
    return 0;
}

// Test data operations with large values
static int test_data_large_values(void) {
    printf("  Running test: data_large_values\n");
    
    // Create large key and value
    char large_key[1024];
    char large_value[1024 * 1024];  // 1MB
    char* buffer = malloc(sizeof(large_value));
    TEST_ASSERT_NOT_NULL(buffer);
    
    size_t size;
    ppdb_error_t err;
    
    memset(large_key, 'K', sizeof(large_key) - 1);
    large_key[sizeof(large_key) - 1] = '\0';
    
    memset(large_value, 'V', sizeof(large_value) - 1);
    large_value[sizeof(large_value) - 1] = '\0';
    
    // Put large data
    err = ppdb_storage_put(table, large_key, strlen(large_key), 
                          large_value, strlen(large_value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    // Get large data
    size = sizeof(large_value);
    err = ppdb_storage_get(table, large_key, strlen(large_key), 
                          buffer, &size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(0, strcmp(buffer, large_value));
    
    // Delete large data
    err = ppdb_storage_delete(table, large_key, strlen(large_key));
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    free(buffer);
    printf("  Test passed: data_large_values\n");
    return 0;
}

// Test multiple data operations
static int test_data_multiple_operations(void) {
    printf("  Running test: data_multiple_operations\n");
    
    const int num_entries = 1000;
    char key[32];
    char value[32];
    char buffer[32];
    size_t size;
    ppdb_error_t err;
    
    // Insert multiple entries
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        err = ppdb_storage_put(table, key, strlen(key), 
                              value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }
    
    // Read all entries
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        size = sizeof(buffer);
        err = ppdb_storage_get(table, key, strlen(key), 
                              buffer, &size);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        TEST_ASSERT_EQUALS(0, strcmp(buffer, value));
    }
    
    // Delete all entries
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        err = ppdb_storage_delete(table, key, strlen(key));
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }
    
    // Verify all entries are deleted
    for (int i = 0; i < num_entries; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        size = sizeof(buffer);
        err = ppdb_storage_get(table, key, strlen(key), 
                              buffer, &size);
        TEST_ASSERT_EQUALS(PPDB_ERR_NOT_FOUND, err);
    }
    
    printf("  Test passed: data_multiple_operations\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    test_setup();
    
    TEST_RUN(test_data_basic_operations);
    TEST_RUN(test_data_invalid_params);
    TEST_RUN(test_data_large_values);
    TEST_RUN(test_data_multiple_operations);
    
    test_teardown();
    
    TEST_CLEANUP();
    return 0;
}