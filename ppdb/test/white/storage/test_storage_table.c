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

// Test table creation
static int test_table_create_normal(void) {
    printf("  Running test: table_create_normal\n");
    
    // Create table
    ppdb_error_t err = ppdb_storage_create_table(storage, "test_table", &table);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(table);
    TEST_ASSERT_NOT_NULL(table->name);
    TEST_ASSERT_EQUALS(0, strcmp(table->name, "test_table"));
    TEST_ASSERT_NOT_NULL(table->data);
    TEST_ASSERT_NOT_NULL(table->indexes);
    TEST_ASSERT_EQUALS(0, table->size);
    TEST_ASSERT_EQUALS(true, table->is_open);
    
    // Try to create same table again
    ppdb_storage_table_t* table2 = NULL;
    err = ppdb_storage_create_table(storage, "test_table", &table2);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_TABLE_EXISTS, err);
    TEST_ASSERT_EQUALS(NULL, table2);
    
    // Create another table with different name
    ppdb_storage_table_t* table3 = NULL;
    err = ppdb_storage_create_table(storage, "test_table2", &table3);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(table3);
    TEST_ASSERT_NOT_NULL(table3->name);
    TEST_ASSERT_EQUALS(0, strcmp(table3->name, "test_table2"));
    TEST_ASSERT_NOT_NULL(table3->data);
    TEST_ASSERT_NOT_NULL(table3->indexes);
    TEST_ASSERT_EQUALS(0, table3->size);
    TEST_ASSERT_EQUALS(true, table3->is_open);
    
    printf("  Test passed: table_create_normal\n");
    return 0;
}

// Test table creation with invalid parameters
static int test_table_create_invalid(void) {
    printf("  Running test: table_create_invalid\n");
    
    // Test NULL parameters
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_create_table(NULL, "test_table", &table));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_create_table(storage, NULL, &table));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_create_table(storage, "test_table", NULL));
    
    // Test invalid table names
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_create_table(storage, "", &table));
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, ppdb_storage_create_table(storage, "   ", &table));
    
    printf("  Test passed: table_create_invalid\n");
    return 0;
}

// Test table operations
static int test_table_operations(void) {
    printf("  Running test: table_operations\n");
    
    ppdb_error_t err;
    table = NULL;  // Initialize table to NULL
    
    // Create table
    err = ppdb_storage_create_table(storage, "test_table", &table);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(table);
    
    // Get table
    ppdb_storage_table_t* table2 = NULL;
    err = ppdb_storage_get_table(storage, "test_table", &table2);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(table2);
    TEST_ASSERT_EQUALS(table, table2);
    
    // Drop table
    err = ppdb_storage_drop_table(storage, "test_table");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    table = NULL;  // Reset table after dropping
    
    // Try to get dropped table
    err = ppdb_storage_get_table(storage, "test_table", &table2);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_TABLE_NOT_FOUND, err);
    
    // Try to drop non-existent table
    err = ppdb_storage_drop_table(storage, "non_existent");
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_TABLE_NOT_FOUND, err);
    
    printf("  Test passed: table_operations\n");
    return 0;
}

// Test multiple tables
static int test_multiple_tables(void) {
    printf("  Running test: multiple_tables\n");
    
    ppdb_error_t err;
    
    // Create multiple tables
    ppdb_storage_table_t* tables[3] = {NULL};
    err = ppdb_storage_create_table(storage, "table1", &tables[0]);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(tables[0]);
    
    err = ppdb_storage_create_table(storage, "table2", &tables[1]);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(tables[1]);
    
    err = ppdb_storage_create_table(storage, "table3", &tables[2]);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(tables[2]);
    
    // Verify all tables exist
    ppdb_storage_table_t* table_check = NULL;
    err = ppdb_storage_get_table(storage, "table1", &table_check);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(tables[0], table_check);
    
    err = ppdb_storage_get_table(storage, "table2", &table_check);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(tables[1], table_check);
    
    err = ppdb_storage_get_table(storage, "table3", &table_check);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(tables[2], table_check);
    
    // Drop tables in different order
    err = ppdb_storage_drop_table(storage, "table2");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    err = ppdb_storage_drop_table(storage, "table1");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    err = ppdb_storage_drop_table(storage, "table3");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    printf("  Test passed: multiple_tables\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    test_setup();
    
    TEST_RUN(test_table_create_normal);
    TEST_RUN(test_table_create_invalid);
    TEST_RUN(test_table_operations);
    TEST_RUN(test_multiple_tables);
    
    test_teardown();
    
    TEST_CLEANUP();
    return 0;
}