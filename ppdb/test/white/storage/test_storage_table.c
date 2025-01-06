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

// Test table creation
static void test_table_create_normal(void) {
    printf("  Running test: table_create_normal\n");
    
    // Create table
    assert(ppdb_storage_create_table(storage, "test_table", &table) == PPDB_OK);
    assert(table != NULL);
    
    // Try to create same table again
    ppdb_storage_table_t* table2 = NULL;
    assert(ppdb_storage_create_table(storage, "test_table", &table2) == PPDB_ERR_ALREADY_EXISTS);
    assert(table2 == NULL);
    
    printf("  Test passed: table_create_normal\n");
}

// Test table creation with invalid parameters
static void test_table_create_invalid(void) {
    printf("  Running test: table_create_invalid\n");
    
    // Test NULL parameters
    assert(ppdb_storage_create_table(NULL, "test_table", &table) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_create_table(storage, NULL, &table) == PPDB_ERR_NULL_POINTER);
    assert(ppdb_storage_create_table(storage, "test_table", NULL) == PPDB_ERR_NULL_POINTER);
    
    // Test invalid table names
    assert(ppdb_storage_create_table(storage, "", &table) == PPDB_ERR_INVALID_ARGUMENT);
    assert(ppdb_storage_create_table(storage, "   ", &table) == PPDB_ERR_INVALID_ARGUMENT);
    
    printf("  Test passed: table_create_invalid\n");
}

// Test table operations
static void test_table_operations(void) {
    printf("  Running test: table_operations\n");
    
    // Create table
    assert(ppdb_storage_create_table(storage, "test_table", &table) == PPDB_OK);
    
    // Get table
    ppdb_storage_table_t* table2 = NULL;
    assert(ppdb_storage_get_table(storage, "test_table", &table2) == PPDB_OK);
    assert(table2 == table);
    
    // Drop table
    assert(ppdb_storage_drop_table(storage, "test_table") == PPDB_OK);
    
    // Try to get dropped table
    assert(ppdb_storage_get_table(storage, "test_table", &table2) == PPDB_ERR_NOT_FOUND);
    
    // Try to drop non-existent table
    assert(ppdb_storage_drop_table(storage, "non_existent") == PPDB_ERR_NOT_FOUND);
    
    printf("  Test passed: table_operations\n");
}

// Test multiple tables
static void test_multiple_tables(void) {
    printf("  Running test: multiple_tables\n");
    
    // Create multiple tables
    ppdb_storage_table_t* tables[3] = {NULL};
    assert(ppdb_storage_create_table(storage, "table1", &tables[0]) == PPDB_OK);
    assert(ppdb_storage_create_table(storage, "table2", &tables[1]) == PPDB_OK);
    assert(ppdb_storage_create_table(storage, "table3", &tables[2]) == PPDB_OK);
    
    // Verify all tables exist
    ppdb_storage_table_t* table_check = NULL;
    assert(ppdb_storage_get_table(storage, "table1", &table_check) == PPDB_OK);
    assert(table_check == tables[0]);
    assert(ppdb_storage_get_table(storage, "table2", &table_check) == PPDB_OK);
    assert(table_check == tables[1]);
    assert(ppdb_storage_get_table(storage, "table3", &table_check) == PPDB_OK);
    assert(table_check == tables[2]);
    
    // Drop tables in different order
    assert(ppdb_storage_drop_table(storage, "table2") == PPDB_OK);
    assert(ppdb_storage_drop_table(storage, "table1") == PPDB_OK);
    assert(ppdb_storage_drop_table(storage, "table3") == PPDB_OK);
    
    printf("  Test passed: multiple_tables\n");
}

int main(void) {
    printf("Running test suite: Storage Table Tests\n");
    
    test_setup();
    
    test_table_create_normal();
    test_table_create_invalid();
    test_table_operations();
    test_multiple_tables();
    
    test_teardown();
    
    printf("Test suite completed\n");
    return 0;
}