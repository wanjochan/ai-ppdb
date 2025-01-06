#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "test_common.h"

// Test setup and teardown
static ppdb_base_t* base = NULL;
static ppdb_storage_t* storage = NULL;
static ppdb_storage_table_t* table = NULL;
static ppdb_storage_index_t* index = NULL;
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
    if (index) {
        // Index will be destroyed by table
        index = NULL;
    }
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

// Test index creation
static void test_index_create_normal(void) {
    printf("  Running test: index_create_normal\n");
    
    // Create index
    assert(ppdb_storage_create_index(table, "test_index", &index) == PPDB_OK);
    assert(index != NULL);
    
    // Try to create same index again
    ppdb_storage_index_t* index2 = NULL;
    assert(ppdb_storage_create_index(table, "test_index", &index2) == PPDB_ERR_ALREADY_EXISTS);
    assert(index2 == NULL);
    
    printf("  Test passed: index_create_normal\n");
}

// Test index creation with invalid parameters
static void test_index_create_invalid(void) {
    printf("  Running test: index_create_invalid\n");
    
    // Test NULL parameters
    assert(ppdb_storage_create_index(NULL, "test_index", &index) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_create_index(table, NULL, &index) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_create_index(table, "test_index", NULL) == PPDB_ERR_NULL_POINTER);
    
    // Test invalid index names
    assert(ppdb_storage_create_index(table, "", &index) == PPDB_ERR_INVALID_ARGUMENT);
    assert(ppdb_storage_create_index(table, "   ", &index) == PPDB_ERR_INVALID_ARGUMENT);
    
    printf("  Test passed: index_create_invalid\n");
}

// Test index operations
static void test_index_operations(void) {
    printf("  Running test: index_operations\n");
    
    // Create index
    assert(ppdb_storage_create_index(table, "test_index", &index) == PPDB_OK);
    
    // Get index
    ppdb_storage_index_t* index2 = NULL;
    assert(ppdb_storage_get_index(table, "test_index", &index2) == PPDB_OK);
    assert(index2 == index);
    
    // Drop index
    assert(ppdb_storage_drop_index(table, "test_index") == PPDB_OK);
    
    // Try to get dropped index
    assert(ppdb_storage_get_index(table, "test_index", &index2) == PPDB_ERR_NOT_FOUND);
    
    // Try to drop non-existent index
    assert(ppdb_storage_drop_index(table, "non_existent") == PPDB_ERR_NOT_FOUND);
    
    printf("  Test passed: index_operations\n");
}

// Test multiple indexes
static void test_multiple_indexes(void) {
    printf("  Running test: multiple_indexes\n");
    
    // Create multiple indexes
    ppdb_storage_index_t* indexes[3] = {NULL};
    assert(ppdb_storage_create_index(table, "index1", &indexes[0]) == PPDB_OK);
    assert(ppdb_storage_create_index(table, "index2", &indexes[1]) == PPDB_OK);
    assert(ppdb_storage_create_index(table, "index3", &indexes[2]) == PPDB_OK);
    
    // Verify all indexes exist
    ppdb_storage_index_t* index_check = NULL;
    assert(ppdb_storage_get_index(table, "index1", &index_check) == PPDB_OK);
    assert(index_check == indexes[0]);
    assert(ppdb_storage_get_index(table, "index2", &index_check) == PPDB_OK);
    assert(index_check == indexes[1]);
    assert(ppdb_storage_get_index(table, "index3", &index_check) == PPDB_OK);
    assert(index_check == indexes[2]);
    
    // Drop indexes in different order
    assert(ppdb_storage_drop_index(table, "index2") == PPDB_OK);
    assert(ppdb_storage_drop_index(table, "index1") == PPDB_OK);
    assert(ppdb_storage_drop_index(table, "index3") == PPDB_OK);
    
    printf("  Test passed: multiple_indexes\n");
}

int main(void) {
    printf("Running test suite: Storage Index Tests\n");
    
    test_setup();
    
    test_index_create_normal();
    test_index_create_invalid();
    test_index_operations();
    test_multiple_indexes();
    
    test_teardown();
    
    printf("Test suite completed\n");
    return 0;
}