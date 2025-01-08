#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "internal/storage.h"
#include "../test_framework.h"

// Test setup and teardown
static ppdb_base_t* base = NULL;
static ppdb_engine_t* engine = NULL;
static ppdb_storage_t* storage = NULL;
static ppdb_storage_table_t* table = NULL;
static ppdb_base_config_t base_config;
static ppdb_storage_config_t storage_config;

static void cleanup_resources(void) {
    ppdb_error_t err;
    
    if (storage && storage->current_tx) {
        printf("Warning: Transaction still active during cleanup, rolling back\n");
        err = ppdb_engine_txn_rollback(storage->current_tx);
        if (err != PPDB_OK) {
            printf("Error: Failed to rollback transaction: %d, error: %s\n", 
                   err, ppdb_error_str(err));
        }
        storage->current_tx = NULL;
    }
    
    if (table) {
        err = ppdb_storage_table_close(table);
        if (err != PPDB_OK) {
            printf("Error: Failed to close table: %d, error: %s\n", err, ppdb_error_str(err));
        }
        table = NULL;
    }
    
    if (storage) {
        err = ppdb_storage_destroy(storage);
        if (err != PPDB_OK) {
            printf("Error: Failed to destroy storage: %d\n", err);
        }
        storage = NULL;
    }
    
    if (engine) {
        ppdb_engine_destroy(engine);
        engine = NULL;
    }
    
    if (base) {
        ppdb_base_destroy(base);
        base = NULL;
    }
}

static int test_setup(void) {
    printf("\n=== Setting up test environment ===\n");
    ppdb_error_t err;
    
    // Initialize base config with safe defaults
    memset(&base_config, 0, sizeof(ppdb_base_config_t));
    base_config.memory_limit = 1024 * 1024;
    base_config.thread_pool_size = 4;
    base_config.thread_safe = true;
    
    // Initialize storage config with safe defaults
    memset(&storage_config, 0, sizeof(ppdb_storage_config_t));
    storage_config.memtable_size = 64 * 1024;
    storage_config.block_size = 4096;
    storage_config.cache_size = 256 * 1024;
    storage_config.write_buffer_size = 64 * 1024;
    storage_config.data_dir = "test_data";
    storage_config.use_compression = true;
    storage_config.sync_writes = true;
    
    printf("Initializing base layer...\n");
    if ((err = ppdb_base_init(&base, &base_config)) != PPDB_OK) {
        printf("Error: Failed to initialize base: %d, error: %s\n", err, ppdb_error_str(err));
        goto cleanup;
    }
    printf("Base layer initialized successfully\n");
    
    if ((err = ppdb_engine_init(&engine, base)) != PPDB_OK) {
        printf("Error: Failed to initialize engine: %d, error: %s\n", err, ppdb_error_str(err));
        goto cleanup;
    }
    printf("Engine layer initialized successfully\n");
    
    if ((err = ppdb_storage_init(&storage, engine, &storage_config)) != PPDB_OK) {
        printf("Error: Failed to initialize storage: %d, error: %s\n", err, ppdb_error_str(err));
        goto cleanup;
    }
    printf("Storage layer initialized successfully\n");
    
    return 0;

cleanup:
    cleanup_resources();
    return -1;
}

static int test_teardown(void) {
    printf("\n=== Tearing down test environment ===\n");
    cleanup_resources();
    return 0;
}

// Test table creation with normal parameters
static int test_table_create_normal(void) {
    printf("\n=== Test Case: test_table_create_normal ===\n");
    ppdb_error_t err;
    ppdb_engine_txn_t* tx = NULL;
    
    // Enhanced pre-transaction validation
    TEST_ASSERT_NULL(storage->current_tx);
    TEST_ASSERT_MEMORY_CLEAN();
    
    printf("Starting transaction for table creation test\n");
    if ((err = ppdb_engine_txn_begin(engine, &tx)) != PPDB_OK) {
        printf("Error: Failed to begin transaction: %d, error: %s\n", 
               err, ppdb_error_str(err));
        return -1;
    }
    
    // Explicit transaction context setting
    storage->current_tx = tx;
    TEST_ASSERT_NOT_NULL(storage->current_tx);
    TEST_ASSERT_TRANSACTION_ACTIVE(tx);
    
    // Post-begin transaction state check
    TEST_ASSERT_TRANSACTION_ACTIVE(tx);
    printf("Transaction began successfully, tx: %p\n", (void*)tx);
    
    printf("Attempting to create table 'test_table'\n");
    if ((err = ppdb_storage_create_table(storage, "test_table", &table)) != PPDB_OK) {
        printf("Error: Failed to create table: %d, error: %s\n", err, ppdb_error_str(err));
        goto rollback;
    }
    TEST_ASSERT_NOT_NULL(table);
    printf("Table 'test_table' created successfully\n");
    
    printf("Committing transaction\n");
    if ((err = ppdb_engine_txn_commit(tx)) != PPDB_OK) {
        printf("Error: Failed to commit transaction: %d, error: %s\n", err, ppdb_error_str(err));
        goto rollback;
    }
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    TEST_ASSERT_MEMORY_CLEAN();
    printf("Transaction committed successfully\n");
    
    // Cleanup phase with transaction checks
    printf("Starting cleanup phase\n");
    TEST_ASSERT_NULL(storage->current_tx);
    err = ppdb_engine_txn_begin(engine, &tx);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    storage->current_tx = tx;
    TEST_ASSERT_TRANSACTION_ACTIVE(tx);
    
    printf("Dropping test table\n");
    err = ppdb_storage_drop_table(storage, "test_table");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    table = NULL;
    
    printf("Committing cleanup transaction\n");
    err = ppdb_engine_txn_commit(tx);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    TEST_ASSERT_MEMORY_CLEAN();
    
    printf("Test test_table_create_normal completed successfully\n");
    return 0;

rollback:
    printf("Warning: Rolling back transaction due to error\n");
    if ((err = ppdb_engine_txn_rollback(tx)) != PPDB_OK) {
        printf("Error: Rollback failed: %d, error: %s\n", err, ppdb_error_str(err));
    }
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    TEST_ASSERT_MEMORY_CLEAN();
    return -1;
}

// Test table creation with invalid parameters
static int test_table_create_invalid(void) {
    printf("\n=== Test Case: test_table_create_invalid ===\n");
    ppdb_error_t err;
    ppdb_engine_txn_t* tx = NULL;
    ppdb_storage_table_t* invalid_table = NULL;
    
    TEST_ASSERT_NULL(storage->current_tx);
    printf("Beginning transaction for invalid table creation tests\n");
    
    if ((err = ppdb_engine_txn_begin(engine, &tx)) != PPDB_OK) {
        printf("Error: Failed to begin transaction: %d, error: %s\n", 
               err, ppdb_error_str(err));
        return -1;
    }
    storage->current_tx = tx;
    
    printf("Testing NULL storage parameter\n");
    err = ppdb_storage_create_table(NULL, "test_table", &invalid_table);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    printf("Testing NULL table name parameter\n");
    err = ppdb_storage_create_table(storage, NULL, &invalid_table);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    printf("Testing empty table name\n");
    err = ppdb_storage_create_table(storage, "", &invalid_table);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    printf("Testing whitespace table name\n");
    err = ppdb_storage_create_table(storage, "   ", &invalid_table);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    // Enhanced transaction cleanup
    printf("Committing transaction\n");
    if ((err = ppdb_engine_txn_commit(tx)) != PPDB_OK) {
        printf("Error: Failed to commit transaction: %d, error: %s\n", 
               err, ppdb_error_str(err));
        goto rollback;
    }
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    
    printf("Test test_table_create_invalid completed successfully\n");
    return 0;

rollback:
    if ((err = ppdb_engine_txn_rollback(tx)) != PPDB_OK) {
        printf("Error: Rollback failed: %d, error: %s\n", err, ppdb_error_str(err));
    }
    storage->current_tx = NULL;
    return -1;
}

// Test table operations
static int test_table_operations(void) {
    printf("\n=== Test Case: test_table_operations ===\n");
    ppdb_error_t err;
    ppdb_engine_txn_t* tx = NULL;
    
    TEST_ASSERT_NULL(storage->current_tx);
    printf("Beginning transaction for table operations test\n");
    
    err = ppdb_engine_txn_begin(engine, &tx);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    storage->current_tx = tx;
    
    printf("Creating test table\n");
    err = ppdb_storage_create_table(storage, "test_table", &table);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(table);
    
    printf("Attempting to create duplicate table\n");
    ppdb_storage_table_t* duplicate_table = NULL;
    err = ppdb_storage_create_table(storage, "test_table", &duplicate_table);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_TABLE_EXISTS, err);
    
    printf("Dropping test table\n");
    err = ppdb_storage_drop_table(storage, "test_table");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    table = NULL;
    
    printf("Attempting to drop non-existent table\n");
    err = ppdb_storage_drop_table(storage, "non_existent");
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_TABLE_NOT_FOUND, err);
    
    printf("Committing transaction\n");
    err = ppdb_engine_txn_commit(tx);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    storage->current_tx = NULL;
    
    printf("Test test_table_operations completed successfully\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    printf("\n=== Starting Storage Table Tests ===\n");
    
    if (test_setup() != 0) {
        printf("Error: Test setup failed\n");
        return -1;
    }
    
    TEST_RUN(test_table_create_normal);
    TEST_RUN(test_table_create_invalid);
    TEST_RUN(test_table_operations);
    
    test_teardown();
    printf("\n=== All storage table tests completed successfully ===\n");
    
    TEST_CLEANUP();
    return 0;
}