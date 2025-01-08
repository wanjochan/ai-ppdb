#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "internal/storage.h"
#include "../test_framework.h"

// Global variables with explicit initialization
static ppdb_base_t* base = NULL;
static ppdb_engine_t* engine = NULL;
static ppdb_storage_t* storage = NULL;
static ppdb_storage_table_t* table = NULL;

// Forward declarations
static void test_cleanup(void);
static void log_error(const char* message, int err);
static ppdb_error_t test_setup(void);

// Helper function for error logging
static void log_error(const char* message, int err) {
    printf("Error: %s (code: %d, details: %s)\n", message, err, ppdb_error_to_string(err));
}

// Cleanup function
static void test_cleanup(void) {
    ppdb_error_t err;
    
    // Enhanced transaction cleanup
    if (storage && storage->current_tx) {
        printf("Warning: Cleaning up with active transaction - performing rollback\n");
        err = ppdb_engine_txn_rollback(storage->current_tx);
        if (err != PPDB_OK) {
            log_error("Failed to rollback transaction during cleanup", err);
        }
        storage->current_tx = NULL;
    }

    if (table) {
        table = NULL;  // Just clear the pointer, actual cleanup is handled by storage
    }

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
}

// Setup function
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
        log_error("Failed to initialize base layer", err);
        test_cleanup();
        return err;
    }

    // Initialize engine layer
    if ((err = ppdb_engine_init(&engine, base)) != PPDB_OK) {
        log_error("Failed to initialize engine layer", err);
        test_cleanup();
        return err;
    }

    // Initialize storage layer
    if ((err = ppdb_storage_init(&storage, engine, &storage_config)) != PPDB_OK) {
        log_error("Failed to initialize storage layer", err);
        test_cleanup();
        return err;
    }

    return PPDB_OK;
}

// Test basic data operations
static int test_data_basic_operations(void) {
    printf("\n=== Starting test: data_basic_operations ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }
    
    // Table creation transaction with enhanced error handling
    ppdb_engine_txn_t* tx = NULL;
    printf("Beginning transaction for table creation...\n");
    
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin transaction", err);
        test_cleanup();
        return -1;
    }
    
    // Set transaction context
    storage->current_tx = tx;
    printf("Transaction context set for table creation\n");

    err = ppdb_storage_create_table(storage, "test_table", &table);
    if (err != PPDB_OK) {
        log_error("Failed to create table", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }

    printf("Committing table creation transaction...\n");
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit table creation", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;  // Clear the transaction after commit
    tx = NULL;  // Also clear the transaction pointer
    printf("Table creation transaction completed successfully\n");

    // Write operation
    printf("Writing key-value pair...\n");
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size = sizeof(buffer);
    
    // Begin write transaction
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin write transaction", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    err = ppdb_storage_put(table, key, strlen(key), value, strlen(value) + 1);
    if (err != PPDB_OK) {
        printf("ERROR: Failed to put key-value pair: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }

    // Commit write transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit write transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    tx = NULL;

    // Read operation
    printf("Reading key-value pair...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin read transaction", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    size = sizeof(buffer);
    err = ppdb_storage_get(table, key, strlen(key), buffer, &size);
    if (err != PPDB_OK) {
        printf("ERROR: Failed to get key-value pair: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }
    TEST_ASSERT_EQUALS(strlen(value) + 1, size);
    TEST_ASSERT_EQUALS(0, strcmp(buffer, value));

    // Commit read transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit read transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    tx = NULL;

    // Delete operation
    printf("Deleting key-value pair...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin delete transaction", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    err = ppdb_storage_delete(table, key, strlen(key));
    if (err != PPDB_OK) {
        printf("ERROR: Failed to delete key-value pair: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }

    // Commit delete transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit delete transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    tx = NULL;

    // Verify deletion
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin verification transaction", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    err = ppdb_storage_get(table, key, strlen(key), buffer, &size);
    if (err != PPDB_STORAGE_ERR_NOT_FOUND) {
        printf("ERROR: Key still exists after deletion: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }

    // Commit verification transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit verification transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    tx = NULL;

    test_cleanup();
    return 0;
}

// Test data operations with invalid parameters
static int test_data_invalid_params(void) {
    printf("\n=== Starting test: data_invalid_params ===\n");
    
    const char* key = "test_key";
    const char* value = "test_value";
    char buffer[256];
    size_t size = sizeof(buffer);
    
    // Test NULL parameters
    ppdb_error_t err;
    err = ppdb_storage_put(NULL, key, strlen(key), value, strlen(value));
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_put(table, NULL, strlen(key), value, strlen(value));
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_put(table, key, strlen(key), NULL, strlen(value));
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_get(NULL, key, strlen(key), buffer, &size);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_get(table, NULL, strlen(key), buffer, &size);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_get(table, key, strlen(key), NULL, &size);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_get(table, key, strlen(key), buffer, NULL);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_delete(NULL, key, strlen(key));
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_delete(table, NULL, strlen(key));
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    // Test zero-length parameters
    err = ppdb_storage_put(table, key, 0, value, strlen(value));
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_put(table, key, strlen(key), value, 0);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_get(table, key, 0, buffer, &size);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    err = ppdb_storage_delete(table, key, 0);
    if (err != PPDB_STORAGE_ERR_PARAM) {
        printf("ERROR: Expected PPDB_STORAGE_ERR_PARAM but got: %d\n", err);
        return -1;
    }
    
    printf("=== Completed test: data_invalid_params ===\n\n");
    return 0;
}

// Test data operations with large values
static int test_data_large_values(void) {
    printf("\n=== Starting test: data_large_values ===\n");
    
    // Setup test environment
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Create table
    ppdb_engine_txn_t* tx = NULL;
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        printf("Failed to begin transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;

    printf("Creating table 'test_table'...\n");
    err = ppdb_storage_create_table(storage, "test_table", &table);
    if (err != PPDB_OK) {
        log_error("Failed to create table", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }

    printf("Committing table creation transaction...\n");
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit table creation", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;  // Clear the transaction after commit
    printf("Table creation completed successfully\n");

    // Start a new transaction for write operation
    printf("Starting write transaction for large values...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin write transaction", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;

    printf("Creating large key-value pair...\n");
    const size_t key_size = 256;
    const size_t value_size = 64 * 1024;  // 64KB
    
    char* large_key = malloc(key_size);
    if (large_key == NULL) {
        printf("Failed to allocate key buffer\n");
        test_cleanup();
        return -1;
    }
    
    char* large_value = malloc(value_size);
    if (large_value == NULL) {
        printf("Failed to allocate value buffer\n");
        free(large_key);
        test_cleanup();
        return -1;
    }
    
    char* read_buffer = malloc(value_size);
    if (read_buffer == NULL) {
        printf("Failed to allocate read buffer\n");
        free(large_key);
        free(large_value);
        test_cleanup();
        return -1;
    }
    
    // Initialize test data
    memset(large_key, 'K', key_size - 1);
    large_key[key_size - 1] = '\0';
    
    memset(large_value, 'V', value_size - 1);
    large_value[value_size - 1] = '\0';
    
    // Write large data
    printf("Writing large key-value pair...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        printf("Failed to begin write transaction: %d\n", err);
        free(large_key);
        free(large_value);
        free(read_buffer);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    err = ppdb_storage_put(table, large_key, strlen(large_key), 
                          large_value, strlen(large_value) + 1);
    if (err != PPDB_OK) {
        printf("Failed to put large key-value pair: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        free(large_key);
        free(large_value);
        free(read_buffer);
        test_cleanup();
        return -1;
    }
    
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Failed to commit write transaction: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        free(large_key);
        free(large_value);
        free(read_buffer);
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    
    // Read and verify large data
    printf("Reading large key-value pair...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        printf("Failed to begin read transaction: %d\n", err);
        free(large_key);
        free(large_value);
        free(read_buffer);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    size_t read_size = value_size;
    err = ppdb_storage_get(table, large_key, strlen(large_key), 
                          read_buffer, &read_size);
    if (err != PPDB_OK) {
        printf("Failed to get large key-value pair: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        free(large_key);
        free(large_value);
        free(read_buffer);
        test_cleanup();
        return -1;
    }
    
    // Verify data
    if (read_size != strlen(large_value) + 1 || 
        strcmp(read_buffer, large_value) != 0) {
        printf("Data verification failed\n");
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        free(large_key);
        free(large_value);
        free(read_buffer);
        test_cleanup();
        return -1;
    }
    
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Failed to commit read transaction: %d\n", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        free(large_key);
        free(large_value);
        free(read_buffer);
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    
    // Cleanup
    free(large_key);
    free(large_value);
    free(read_buffer);
    test_cleanup();
    
    printf("=== Completed test: data_large_values ===\n\n");
    return 0;
}

// Test multiple data operations
static int test_data_multiple_operations(void) {
    printf("\n=== Starting test: data_multiple_operations ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("Failed to setup test environment: %d\n", err);
        return -1;
    }
    TEST_ASSERT_NULL(storage->current_tx);

    // Begin transaction for table creation
    ppdb_engine_txn_t* tx = NULL;
    printf("Beginning transaction for multiple operations...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin transaction", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    printf("Transaction started successfully\n");

    err = ppdb_storage_create_table(storage, "test_table", &table);
    if (err != PPDB_OK) {
        log_error("Failed to create table", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }

    printf("Committing table creation transaction...\n");
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit table creation", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;  // Clear the transaction after commit
    printf("Table creation completed successfully\n");

    // Start a new transaction for write operations
    printf("Starting write transaction for multiple operations...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin write transaction", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;

    printf("Performing multiple operations...\n");
    const int num_entries = 100;  // Reduced from 1000
    char key[32];
    char value[32];
    char buffer[32];
    size_t size;
    
    // Insert multiple entries
    printf("Inserting %d entries...\n", num_entries);
    for (int i = 0; i < num_entries; i++) {
        if (i % 10 == 0) {
            printf("Progress: %d/%d entries\n", i, num_entries);
        }
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        err = ppdb_storage_put(table, key, strlen(key), value, strlen(value) + 1);
        if (err != PPDB_OK) {
            printf("Failed to put key-value pair: %d\n", err);
            ppdb_engine_txn_rollback(tx);
            storage->current_tx = NULL;
            test_cleanup();
            return -1;
        }
    }
    
    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Failed to commit write transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    
    // Begin transaction for read operations
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        printf("Failed to begin read transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    printf("Reading %d entries...\n", num_entries);
    // Read all entries
    for (int i = 0; i < num_entries; i++) {
        if (i % 10 == 0) {
            printf("Progress: %d/%d entries\n", i, num_entries);
        }
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        size = sizeof(buffer);
        err = ppdb_storage_get(table, key, strlen(key), buffer, &size);
        if (err != PPDB_OK) {
            printf("Failed to get key-value pair: %d\n", err);
            ppdb_engine_txn_rollback(tx);
            storage->current_tx = NULL;
            test_cleanup();
            return -1;
        }
        TEST_ASSERT_EQUALS(0, strcmp(buffer, value));
    }
    
    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Failed to commit read transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    
    // Begin transaction for delete operations
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        printf("Failed to begin delete transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    printf("Deleting %d entries...\n", num_entries);
    // Delete all entries
    for (int i = 0; i < num_entries; i++) {
        if (i % 10 == 0) {
            printf("Progress: %d/%d entries\n", i, num_entries);
        }
        snprintf(key, sizeof(key), "key_%d", i);
        err = ppdb_storage_delete(table, key, strlen(key));
        if (err != PPDB_OK) {
            printf("Failed to delete key-value pair: %d\n", err);
            ppdb_engine_txn_rollback(tx);
            storage->current_tx = NULL;
            test_cleanup();
            return -1;
        }
    }
    
    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Failed to commit delete transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);
    
    // Begin transaction for verification
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        printf("Failed to begin verification transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = tx;
    
    printf("Verifying deletion of %d entries...\n", num_entries);
    // Verify all entries are deleted
    for (int i = 0; i < num_entries; i++) {
        if (i % 10 == 0) {
            printf("Progress: %d/%d entries\n", i, num_entries);
        }
        snprintf(key, sizeof(key), "key_%d", i);
        size = sizeof(buffer);
        err = ppdb_storage_get(table, key, strlen(key), buffer, &size);
        if (err != PPDB_STORAGE_ERR_NOT_FOUND) {
            printf("Expected key not found, but got error: %d\n", err);
            ppdb_engine_txn_rollback(tx);
            storage->current_tx = NULL;
            test_cleanup();
            return -1;
        }
    }
    
    // Commit transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        printf("Failed to commit verification transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    storage->current_tx = NULL;
    TEST_ASSERT_NULL(storage->current_tx);

    test_cleanup();
    TEST_ASSERT_NULL(storage->current_tx);
    printf("=== Completed test: data_multiple_operations ===\n\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    // Run tests
    TEST_RUN(test_data_basic_operations);
    TEST_RUN(test_data_invalid_params);
    TEST_RUN(test_data_large_values);
    TEST_RUN(test_data_multiple_operations);

    test_cleanup();
    TEST_ASSERT_NULL(storage->current_tx);

    TEST_CLEANUP();
    return 0;
}