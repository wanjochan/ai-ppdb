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

// Forward declarations
static void cleanup_resources(void);
static ppdb_error_t test_setup(void);
static int test_flush_operations(void);
static int test_compaction_operations(void);

// Helper function to cleanup resources
static void cleanup_resources(void) {
    ppdb_error_t err;
    
    // Cleanup any active transaction
    if (storage && storage->current_tx) {
        err = ppdb_engine_txn_rollback(storage->current_tx);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to rollback transaction: %d\n", err);
        }
        storage->current_tx = NULL;
    }

    // Drop test table if exists
    if (table) {
        ppdb_engine_txn_t* tx = NULL;
        err = ppdb_engine_txn_begin(engine, true, &tx);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to begin cleanup transaction: %d\n", err);
            goto cleanup;
        }
        storage->current_tx = tx;

        err = ppdb_storage_drop_table(storage, "test_table");
        if (err != PPDB_OK) {
            printf("ERROR: Failed to drop table: %d\n", err);
            ppdb_engine_txn_rollback(tx);
            storage->current_tx = NULL;
            goto cleanup;
        }

        err = ppdb_engine_txn_commit(tx);
        if (err != PPDB_OK) {
            printf("ERROR: Failed to commit cleanup: %d\n", err);
            ppdb_engine_txn_rollback(tx);
            storage->current_tx = NULL;
            goto cleanup;
        }
        storage->current_tx = NULL;
        table = NULL;
    }

cleanup:
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
        printf("ERROR: Base initialization failed: %d\n", err);
        cleanup_resources();
        return err;
    }

    // Initialize engine layer
    if ((err = ppdb_engine_init(&engine, base)) != PPDB_OK) {
        printf("ERROR: Engine initialization failed: %d\n", err);
        cleanup_resources();
        return err;
    }

    // Initialize storage layer
    if ((err = ppdb_storage_init(&storage, engine, &storage_config)) != PPDB_OK) {
        printf("ERROR: Storage initialization failed: %d\n", err);
        cleanup_resources();
        return err;
    }

    // Create test table
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(engine, true, &tx);
    if (err != PPDB_OK) {
        printf("ERROR: Transaction begin failed: %d\n", err);
        cleanup_resources();
        return err;
    }
    storage->current_tx = tx;

    err = ppdb_storage_create_table(storage, "test_table", &table);
    if (err != PPDB_OK) {
        printf("ERROR: Table creation failed: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return err;
    }

    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("ERROR: Transaction commit failed: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return err;
    }
    storage->current_tx = NULL;

    return PPDB_OK;
}

// Test flush operations
static int test_flush_operations(void) {
    printf("\n=== Starting test: flush_operations ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Begin transaction
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(engine, true, &tx);
    if (err != PPDB_OK) {
        printf("ERROR: Transaction begin failed: %d\n", err);
        cleanup_resources();
        return -1;
    }
    storage->current_tx = tx;

    // Write some test data
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_storage_put(table, key, strlen(key) + 1, value, strlen(value) + 1);
    if (err != PPDB_OK) {
        printf("ERROR: Failed to put data: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }

    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("ERROR: Transaction commit failed: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        cleanup_resources();
        return -1;
    }
    storage->current_tx = NULL;

    cleanup_resources();
    return 0;
}

// Test compaction operations
static int test_compaction_operations(void) {
    printf("\n=== Starting test: compaction_operations ===\n");
    
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

    // TODO: Implement compaction test

    cleanup_resources();
    return 0;
}

int main(void) {
    TEST_INIT();
    
    // Run tests
    TEST_RUN(test_flush_operations);
    TEST_RUN(test_compaction_operations);

    TEST_CLEANUP();
    return 0;
}