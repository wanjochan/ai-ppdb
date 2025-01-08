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
    printf("Read value: '%s', size: %zu\n", buffer, size);
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

    // Prepare test data
    const size_t key_size = 256;
    const size_t value_size = 64 * 1024;  // 64KB
    
    char* large_key = NULL;
    char* large_value = NULL;
    char* read_buffer = NULL;
    size_t read_size = value_size;

    // Allocate memory for key and value
    large_key = malloc(key_size);
    if (large_key == NULL) {
        printf("Failed to allocate key buffer\n");
        goto cleanup;
    }
    
    large_value = malloc(value_size);
    if (large_value == NULL) {
        printf("Failed to allocate value buffer\n");
        goto cleanup;
    }

    read_buffer = malloc(value_size);
    if (read_buffer == NULL) {
        printf("Failed to allocate read buffer\n");
        goto cleanup;
    }

    // Initialize key and value with test data
    memset(large_key, 'K', key_size - 1);
    large_key[key_size - 1] = '\0';
    memset(large_value, 'V', value_size - 1);
    large_value[value_size - 1] = '\0';

    // Start write transaction
    printf("Starting write transaction...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin write transaction", err);
        goto cleanup;
    }
    storage->current_tx = tx;

    // Write large value
    printf("Writing large value...\n");
    err = ppdb_storage_put(table, large_key, key_size, large_value, value_size);
    if (err != PPDB_OK) {
        printf("Failed to write large value: %d\n", err);
        goto cleanup;
    }

    // Commit write transaction
    err = ppdb_engine_txn_commit(storage->current_tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit write transaction", err);
        ppdb_engine_txn_rollback(storage->current_tx);
        storage->current_tx = NULL;
        goto cleanup;
    }
    storage->current_tx = NULL;

    // Start read transaction
    printf("Starting read transaction...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin read transaction", err);
        goto cleanup;
    }
    storage->current_tx = tx;

    // Read back large value
    printf("Reading back large value...\n");
    err = ppdb_storage_get(table, large_key, key_size, read_buffer, &read_size);
    if (err != PPDB_OK) {
        printf("Failed to read large value: %d\n", err);
        goto cleanup;
    }

    // Verify read data
    if (read_size != value_size || memcmp(large_value, read_buffer, value_size) != 0) {
        printf("Read data does not match written data\n");
        goto cleanup;
    }

    // Commit read transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit read transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        goto cleanup;
    }
    storage->current_tx = NULL;

    // Success
    free(large_key);
    free(large_value);
    free(read_buffer);
    test_cleanup();
    return 0;

cleanup:
    // Cleanup on error
    if (storage->current_tx) {
        ppdb_engine_txn_rollback(storage->current_tx);
        storage->current_tx = NULL;
    }
    if (large_key) free(large_key);
    if (large_value) free(large_value);
    if (read_buffer) free(read_buffer);
    test_cleanup();
    return -1;
}

// Test multiple data operations in sequence
static int test_data_multiple_operations(void) {
    printf("\n=== Starting test: data_multiple_operations ===\n");
    
    // Setup test environment
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Create table
    ppdb_engine_txn_t* tx = NULL;
    printf("Beginning transaction for multiple operations...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        printf("Failed to begin transaction: %d\n", err);
        test_cleanup();
        return -1;
    }
    printf("Transaction started successfully\n");
    storage->current_tx = tx;

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

    // Prepare test data
    const int num_pairs = 10;
    char** keys = NULL;
    char** values = NULL;
    char* read_buffer = NULL;
    size_t read_size;

    // Allocate memory for test data
    keys = malloc(num_pairs * sizeof(char*));
    if (!keys) {
        printf("Failed to allocate keys array\n");
        goto cleanup;
    }
    memset(keys, 0, num_pairs * sizeof(char*));

    values = malloc(num_pairs * sizeof(char*));
    if (!values) {
        printf("Failed to allocate values array\n");
        goto cleanup;
    }
    memset(values, 0, num_pairs * sizeof(char*));

    read_buffer = malloc(256);  // Buffer for reading values
    if (!read_buffer) {
        printf("Failed to allocate read buffer\n");
        goto cleanup;
    }

    // Initialize test data
    for (int i = 0; i < num_pairs; i++) {
        keys[i] = malloc(32);
        values[i] = malloc(32);
        if (!keys[i] || !values[i]) {
            printf("Failed to allocate key-value pair %d\n", i);
            goto cleanup;
        }
        snprintf(keys[i], 32, "key_%d", i);
        snprintf(values[i], 32, "value_%d", i);
    }

    // Start write transaction
    printf("Starting write transaction...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin write transaction", err);
        goto cleanup;
    }
    storage->current_tx = tx;

    // Write all key-value pairs
    printf("Writing multiple key-value pairs...\n");
    for (int i = 0; i < num_pairs; i++) {
        err = ppdb_storage_put(table, keys[i], strlen(keys[i]) + 1, 
                             values[i], strlen(values[i]) + 1);
        if (err != PPDB_OK) {
            printf("Failed to write pair %d: %d\n", i, err);
            goto cleanup;
        }
    }

    // Commit write transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit write transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        goto cleanup;
    }
    storage->current_tx = NULL;

    // Start read transaction
    printf("Starting read transaction...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin read transaction", err);
        goto cleanup;
    }
    storage->current_tx = tx;

    // Read and verify all pairs
    printf("Reading and verifying pairs...\n");
    for (int i = 0; i < num_pairs; i++) {
        read_size = 256;
        err = ppdb_storage_get(table, keys[i], strlen(keys[i]) + 1, 
                             read_buffer, &read_size);
        if (err != PPDB_OK) {
            printf("Failed to read pair %d: %d\n", i, err);
            goto cleanup;
        }
        if (strcmp(read_buffer, values[i]) != 0) {
            printf("Data verification failed for pair %d\n", i);
            goto cleanup;
        }
    }

    // Commit read transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit read transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        goto cleanup;
    }
    storage->current_tx = NULL;

    // Start delete transaction
    printf("Starting delete transaction...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin delete transaction", err);
        goto cleanup;
    }
    storage->current_tx = tx;

    // Delete all pairs
    printf("Deleting all pairs...\n");
    for (int i = 0; i < num_pairs; i++) {
        err = ppdb_storage_delete(table, keys[i], strlen(keys[i]) + 1);
        if (err != PPDB_OK) {
            printf("Failed to delete pair %d: %d\n", i, err);
            goto cleanup;
        }
    }

    // Commit delete transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit delete transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        goto cleanup;
    }
    storage->current_tx = NULL;

    // Start verification transaction
    printf("Starting verification transaction...\n");
    err = ppdb_engine_txn_begin(engine, &tx);
    if (err != PPDB_OK) {
        log_error("Failed to begin verification transaction", err);
        goto cleanup;
    }
    storage->current_tx = tx;

    // Verify all pairs are deleted
    printf("Verifying deletions...\n");
    for (int i = 0; i < num_pairs; i++) {
        read_size = 256;
        err = ppdb_storage_get(table, keys[i], strlen(keys[i]) + 1, 
                             read_buffer, &read_size);
        if (err != PPDB_STORAGE_ERR_NOT_FOUND) {
            printf("Key %d still exists after deletion\n", i);
            goto cleanup;
        }
    }

    // Commit verification transaction
    err = ppdb_engine_txn_commit(tx);
    if (err != PPDB_OK) {
        log_error("Failed to commit verification transaction", err);
        ppdb_engine_txn_rollback(tx);
        storage->current_tx = NULL;
        goto cleanup;
    }
    storage->current_tx = NULL;

    // Success - cleanup
    for (int i = 0; i < num_pairs; i++) {
        if (keys[i]) free(keys[i]);
        if (values[i]) free(values[i]);
    }
    free(keys);
    free(values);
    free(read_buffer);
    test_cleanup();
    return 0;

cleanup:
    // Cleanup on error
    if (storage->current_tx) {
        ppdb_engine_txn_rollback(storage->current_tx);
        storage->current_tx = NULL;
    }
    if (keys) {
        for (int i = 0; i < num_pairs; i++) {
            if (keys[i]) free(keys[i]);
        }
        free(keys);
    }
    if (values) {
        for (int i = 0; i < num_pairs; i++) {
            if (values[i]) free(values[i]);
        }
        free(values);
    }
    if (read_buffer) free(read_buffer);
    test_cleanup();
    return -1;
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