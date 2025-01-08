#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "internal/storage.h"
#include "../test_framework.h"

// Global variables
static ppdb_base_t* base = NULL;
static ppdb_engine_t* engine = NULL;
static ppdb_storage_t* storage = NULL;
static ppdb_storage_table_t* table = NULL;

// Helper function to cleanup resources
static void cleanup_resources(void) {
    ppdb_error_t err;
    
    // Cleanup any active transaction
    if (storage && storage->current_tx) {
        err = ppdb_engine_txn_rollback(storage->current_tx);
        if (err != PPDB_OK) {
            printf("Error: Failed to rollback transaction: %d\n", err);
        }
        storage->current_tx = NULL;
    }

    // Close table if open
    if (table) {
        ppdb_storage_table_destroy(table);
        table = NULL;
    }

    // Destroy storage
    if (storage) {
        ppdb_storage_destroy(storage);
        storage = NULL;
    }

    // Destroy engine
    if (engine) {
        ppdb_engine_destroy(engine);
        engine = NULL;
    }

    // Destroy base
    if (base) {
        ppdb_base_destroy(base);
        base = NULL;
    }
}

// Helper function to setup test environment
static ppdb_error_t test_setup(void) {
    ppdb_error_t err;
    
    // Reset all global pointers
    base = NULL;
    engine = NULL;
    storage = NULL;
    table = NULL;

    // Initialize base config with safe defaults
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .thread_pool_size = 4,
        .thread_safe = true
    };

    // Initialize storage config
    ppdb_storage_config_t storage_config = {
        .memtable_size = (size_t)PPDB_DEFAULT_MEMTABLE_SIZE,
        .block_size = (size_t)PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = (size_t)PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = (size_t)PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = true,
        .sync_writes = true
    };

    // Initialize base layer
    if ((err = ppdb_base_init(&base, &base_config)) != PPDB_OK) {
        printf("Error: Failed to initialize base: %d\n", err);
        cleanup_resources();
        return err;
    }

    // Initialize engine layer
    if ((err = ppdb_engine_init(&engine, base)) != PPDB_OK) {
        printf("Error: Failed to initialize engine: %d\n", err);
        cleanup_resources();
        return err;
    }

    // Initialize storage layer
    if ((err = ppdb_storage_init(&storage, engine, &storage_config)) != PPDB_OK) {
        printf("Error: Failed to initialize storage: %d\n", err);
        cleanup_resources();
        return err;
    }

    return PPDB_OK;
}

// Test normal table creation
static int test_table_create_normal(void) {
    printf("\n=== Starting test: table_create_normal ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    if ((err = ppdb_engine_txn_begin(engine, true, &tx)) != PPDB_OK) {
        printf("Error: Failed to begin transaction: %d\n", err);
        cleanup_resources();
        return -1;
    }
    storage->current_tx = tx;

    // Create table
    err = ppdb_storage_create_table(storage, "test_table", &table);
    if (err != PPDB_OK) {
        printf("Error: Failed to create table: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Error: Failed to commit transaction: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }
    storage->current_tx = NULL;

    // Verify table exists
    err = ppdb_engine_txn_begin(engine, true, &tx);
    if (err != PPDB_OK) {
        printf("Error: Failed to begin verification transaction: %d\n", err);
        cleanup_resources();
        return -1;
    }
    storage->current_tx = tx;

    ppdb_storage_table_t* verify_table = NULL;
    err = ppdb_storage_get_table(storage, "test_table", &verify_table);
    if (err != PPDB_OK) {
        printf("Error: Failed to get table: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }

    // Rollback verification transaction
    err = ppdb_engine_txn_rollback(tx);
    if (err != PPDB_OK) {
        printf("Error: Rollback failed: %d\n", err);
        cleanup_resources();
        return -1;
    }
    storage->current_tx = NULL;

    cleanup_resources();
    return 0;
}

// Test invalid table creation
static int test_table_create_invalid(void) {
    printf("\n=== Starting test: table_create_invalid ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    if ((err = ppdb_engine_txn_begin(engine, true, &tx)) != PPDB_OK) {
        printf("Error: Failed to begin transaction: %d\n", err);
        cleanup_resources();
        return -1;
    }
    storage->current_tx = tx;

    // Test NULL parameters
    err = ppdb_storage_create_table(NULL, "test_table", &table);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("Error: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }

    err = ppdb_storage_create_table(storage, NULL, &table);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("Error: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }

    err = ppdb_storage_create_table(storage, "test_table", NULL);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("Error: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Error: Failed to commit transaction: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }
    storage->current_tx = NULL;

    // Rollback verification transaction
    err = ppdb_engine_txn_rollback(tx);
    if (err != PPDB_OK) {
        printf("Error: Rollback failed: %d\n", err);
        cleanup_resources();
        return -1;
    }
    storage->current_tx = NULL;

    cleanup_resources();
    return 0;
}

// Test table operations
static int test_table_operations(void) {
    printf("\n=== Starting test: table_operations ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(engine, true, &tx);
    if (err != PPDB_OK) {
        printf("Failed to begin transaction: %d\n", err);
        cleanup_resources();
        return -1;
    }
    storage->current_tx = tx;

    // Create table
    err = ppdb_storage_create_table(storage, "test_table", &table);
    if (err != PPDB_OK) {
        printf("Failed to create table: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Failed to commit transaction: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }
    storage->current_tx = NULL;

    cleanup_resources();
    return 0;
}

int main(void) {
    TEST_INIT();
    
    // Run tests
    TEST_RUN(test_table_create_normal);
    TEST_RUN(test_table_create_invalid);
    TEST_RUN(test_table_operations);

    TEST_CLEANUP();
    return 0;
}