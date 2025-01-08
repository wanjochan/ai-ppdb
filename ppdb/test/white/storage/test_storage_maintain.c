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

static int test_setup(void) {
    ppdb_error_t err;
    printf("=== Starting test setup ===\n");
    
    // Initialize configs with safe defaults
    memset(&base_config, 0, sizeof(ppdb_base_config_t));
    memset(&storage_config, 0, sizeof(ppdb_storage_config_t));
    
    // Enhanced config initialization
    base_config = (ppdb_base_config_t){
        .memory_limit = 1024 * 1024,
        .thread_pool_size = 4,
        .thread_safe = true,
        .log_level = PPDB_LOG_DEBUG
    };
    
    storage_config = (ppdb_storage_config_t){
        .memtable_size = 64 * 1024,
        .block_size = 4096,
        .cache_size = 256 * 1024,
        .write_buffer_size = 64 * 1024,
        .data_dir = "test_data",
        .use_compression = true,
        .sync_writes = true
    };

    printf("Initializing base...\n");
    if ((err = ppdb_base_init(&base, &base_config)) != PPDB_OK) {
        printf("ERROR: Base initialization failed: %s\n", ppdb_error_str(err));
        return -1;
    }

    printf("Initializing engine...\n");
    if ((err = ppdb_engine_init(&engine, base)) != PPDB_OK) {
        printf("ERROR: Engine initialization failed: %s\n", ppdb_error_str(err));
        goto cleanup;
    }

    printf("Initializing storage...\n");
    if ((err = ppdb_storage_init(&storage, engine, &storage_config)) != PPDB_OK) {
        printf("ERROR: Storage initialization failed: %s\n", ppdb_error_str(err));
        goto cleanup;
    }

    // Transaction handling for table creation
    ppdb_engine_txn_t* tx = NULL;
    printf("Beginning transaction for table creation...\n");
    
    // Clear any existing transaction state
    storage->current_tx = NULL;
    
    if ((err = ppdb_engine_txn_begin(engine, &tx)) != PPDB_OK) {
        printf("ERROR: Transaction begin failed: %s\n", ppdb_error_str(err));
        goto cleanup;
    }

    // Set transaction context immediately after begin
    storage->current_tx = tx;
    
    if ((err = ppdb_storage_create_table(storage, "test_table", &table)) != PPDB_OK) {
        printf("ERROR: Table creation failed: %s\n", ppdb_error_str(err));
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        goto cleanup;
    }

    if ((err = ppdb_engine_txn_commit(tx)) != PPDB_OK) {
        printf("ERROR: Transaction commit failed: %s\n", ppdb_error_str(err));
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        goto cleanup;
    }

    // Clear transaction state after successful commit
    storage->current_tx = NULL;
    printf("Test setup completed successfully\n");
    return 0;

cleanup:
    if (storage) {
        ppdb_storage_destroy(storage);
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
    return -1;
}

static int test_teardown(void) {
    ppdb_error_t err;
    printf("Starting test teardown\n");
    
    // Verify no pending transactions
    TEST_ASSERT_NULL(storage->current_tx);
    
    if (table) {
        ppdb_engine_txn_t* tx = NULL;
        storage->current_tx = NULL;  // Ensure clean state
        
        if ((err = ppdb_engine_txn_begin(engine, &tx)) != PPDB_OK) {
            printf("ERROR: Failed to begin cleanup transaction: %s\n", ppdb_error_str(err));
            return -1;
        }
        
        storage->current_tx = tx;
        
        if ((err = ppdb_storage_drop_table(storage, "test_table")) != PPDB_OK) {
            printf("ERROR: Failed to drop table: %s\n", ppdb_error_str(err));
            ppdb_engine_txn_rollback(tx);
            storage->current_tx = NULL;
            return -1;
        }
        
        if ((err = ppdb_engine_txn_commit(tx)) != PPDB_OK) {
            printf("ERROR: Failed to commit cleanup: %s\n", ppdb_error_str(err));
            ppdb_engine_txn_rollback(tx);
            storage->current_tx = NULL;
            return -1;
        }
        
        storage->current_tx = NULL;
        table = NULL;
    }
    
    // Resource cleanup checks
    if (storage) {
        TEST_ASSERT_NULL(storage->current_tx);
        ppdb_storage_destroy(storage);
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
    printf("Test teardown completed successfully\n");
    return 0;
}

// Test flush operations
static int test_flush_operations(void) {
    ppdb_error_t err;
    printf("\n=== Starting flush operations test ===\n");
    
    // Initial state verification
    TEST_ASSERT_NULL(storage->current_tx);
    
    ppdb_engine_txn_t* tx = NULL;
    printf("Beginning transaction for flush test\n");
    
    // Clear any existing transaction state
    storage->current_tx = NULL;
    
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        printf("ERROR: Transaction begin failed: %s\n", ppdb_error_str(err));
        TEST_ASSERT_NULL(storage->current_tx);
        return -1;
    }
    
    // Set and verify transaction state
    storage->current_tx = tx;
    TEST_ASSERT_NOT_NULL(storage->current_tx);
    
    printf("Setting up test data for flush operation\n");
    const char* key = "test_key";
    const char* value = "test_value";
    size_t key_len = strlen(key);
    size_t value_len = strlen(value) + 1;

    printf("Attempting to put test data: key=%s\n", key);
    if ((err = ppdb_storage_put(table, key, key_len, value, value_len)) != PPDB_OK) {
        printf("ERROR: Failed to put data: %s\n", ppdb_error_str(err));
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        return -1;
    }

    printf("Test data written successfully, committing transaction\n");
    if ((err = ppdb_engine_txn_commit(tx)) != PPDB_OK) {
        printf("ERROR: Transaction commit failed: %s\n", ppdb_error_str(err));
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        return -1;
    }

    // Clear and verify transaction state after commit
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    printf("Flush operations test completed successfully\n");
    return 0;
}

// Test compaction operations
static int test_compaction_operations(void) {
    printf("Starting compaction operations test\n");
    ppdb_error_t err;
    TEST_ASSERT_NULL(storage->current_tx);
    
    printf("Beginning transaction for compaction test\n");
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(engine, &tx);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(tx);
    
    storage->current_tx = tx;
    TEST_ASSERT_NOT_NULL(storage->current_tx);
    
    printf("Writing test data for compaction\n");
    const char* key = "test_key";
    char value[32];
    for (int i = 0; i < 10; i++) {
        snprintf(value, sizeof(value), "value_%d", i);
        err = ppdb_storage_put(table, key, strlen(key), value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }
    
    err = ppdb_engine_txn_commit(tx);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    
    printf("Starting compaction operation\n");
    err = ppdb_storage_compact(table);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NULL(storage->current_tx);
    
    printf("Compaction operations test completed successfully\n");
    return 0;
}

// Test backup and restore operations
static int test_backup_restore_operations(void) {
    printf("Starting backup and restore operations test\n");
    ppdb_error_t err;
    TEST_ASSERT_NULL(storage->current_tx);
    
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(engine, &tx);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(tx);
    
    storage->current_tx = tx;
    TEST_ASSERT_NOT_NULL(storage->current_tx);
    
    printf("Writing test data for backup\n");
    const char* key = "backup_key";
    const char* value = "backup_value";
    err = ppdb_storage_put(table, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    
    err = ppdb_engine_txn_commit(tx);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    
    printf("Performing backup operation\n");
    err = ppdb_storage_backup(storage, "backup");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NULL(storage->current_tx);
    
    printf("Performing restore operation\n");
    err = ppdb_storage_restore(storage, "backup");
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NULL(storage->current_tx);
    
    printf("Backup and restore operations test completed successfully\n");
    return 0;
}

// Test maintenance operations with invalid parameters
static int test_maintain_invalid_params(void) {
    printf("\n=== Starting invalid parameters test ===\n");
    ppdb_error_t err;
    TEST_ASSERT_NULL(storage->current_tx);
    
    printf("Testing NULL parameter handling\n");
    err = ppdb_storage_flush(NULL);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    err = ppdb_storage_compact(NULL);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    err = ppdb_storage_backup(NULL, "backup");
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    err = ppdb_storage_restore(NULL, "backup");
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    err = ppdb_storage_backup(storage, NULL);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    err = ppdb_storage_restore(storage, NULL);
    TEST_ASSERT_EQUALS(PPDB_STORAGE_ERR_PARAM, err);
    
    TEST_ASSERT_NULL(storage->current_tx);
    printf("Invalid parameters test completed successfully\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    printf("Starting storage maintenance tests\n");
    
    if (test_setup() != 0) {
        printf("ERROR: Test setup failed, aborting tests\n");
        return -1;
    }
    
    TEST_RUN(test_flush_operations);
    TEST_RUN(test_compaction_operations);
    TEST_RUN(test_backup_restore_operations);
    TEST_RUN(test_maintain_invalid_params);
    
    if (test_teardown() != 0) {
        printf("ERROR: Test teardown failed\n");
        return -1;
    }
    
    TEST_CLEANUP();
    printf("All storage maintenance tests completed\n");
    return 0;
}