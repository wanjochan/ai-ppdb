/*
 * test_database_index.c - Database Index Tests
 */

#include <cosmopolitan.h>
#include "internal/database.h"
#include "../test_framework.h"

// Global variables
static ppdb_database_t* db = NULL;
static ppdb_database_config_t config;

// Helper function for cleanup
static void cleanup_resources(void) {
    if (db) {
        ppdb_database_destroy(db);
        db = NULL;
    }
}

// Helper function for setup
static ppdb_error_t test_setup(void) {
    // Reset global pointer
    db = NULL;

    // Initialize config with safe defaults
    config = (ppdb_database_config_t){
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .cache_size = 1024 * 1024,         // 1MB
        .enable_mvcc = true,
        .enable_logging = true,
        .sync_on_commit = true,
        .default_isolation = PPDB_TXN_SERIALIZABLE,
        .lock_timeout_ms = 1000,
        .txn_timeout_ms = 5000
    };

    // Initialize database
    ppdb_error_t err = ppdb_database_init(&db, &config);
    if (err != PPDB_OK) {
        printf("ERROR: Failed to initialize database: %d\n", err);
        return err;
    }

    return PPDB_OK;
}

// Custom compare function for testing
static int test_compare(const void* a, const void* b) {
    return memcmp(a, b, strlen(a) < strlen(b) ? strlen(a) : strlen(b));
}

// Test basic index operations
static int test_index_basic(void) {
    printf("\n=== Starting test: index_basic ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Start transaction
    ppdb_txn_t* txn = NULL;
    err = ppdb_txn_begin(db, &txn, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Create index
    err = ppdb_index_create(txn, "test_index", test_compare);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Try to create same index again
    err = ppdb_index_create(txn, "test_index", test_compare);
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_INDEX, err);

    // Drop index
    err = ppdb_index_drop(txn, "test_index");
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Try to drop non-existent index
    err = ppdb_index_drop(txn, "non_existent");
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_NOT_FOUND, err);

    // Commit transaction
    err = ppdb_txn_commit(txn);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: index_basic\n");
    return 0;
}

// Test index lookups
static int test_index_lookup(void) {
    printf("\n=== Starting test: index_lookup ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Start transaction
    ppdb_txn_t* txn = NULL;
    err = ppdb_txn_begin(db, &txn, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Create index
    err = ppdb_index_create(txn, "test_index", test_compare);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Put some data
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_put(txn, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Look up data through index
    void* result;
    size_t result_size;
    err = ppdb_index_get(txn, "test_index", key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen(value) + 1, result_size);
    TEST_ASSERT_EQUALS(0, memcmp(value, result, result_size));
    ppdb_base_free(result);

    // Delete data
    err = ppdb_delete(txn, key, strlen(key));
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Verify index is updated
    err = ppdb_index_get(txn, "test_index", key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_NOT_FOUND, err);

    // Commit transaction
    err = ppdb_txn_commit(txn);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: index_lookup\n");
    return 0;
}

// Test index iteration
static int test_index_iterator(void) {
    printf("\n=== Starting test: index_iterator ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Start transaction
    ppdb_txn_t* txn = NULL;
    err = ppdb_txn_begin(db, &txn, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Create index
    err = ppdb_index_create(txn, "test_index", test_compare);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Put some data
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    for (size_t i = 0; i < 3; i++) {
        err = ppdb_put(txn, keys[i], strlen(keys[i]), values[i], strlen(values[i]) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    // Create iterator
    ppdb_iterator_t* iter = NULL;
    err = ppdb_iterator_create(txn, "test_index", &iter);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Iterate forward
    size_t count = 0;
    while (ppdb_iterator_valid(iter)) {
        void* key;
        void* value;
        size_t key_size, value_size;

        err = ppdb_iterator_key(iter, &key, &key_size);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        TEST_ASSERT_EQUALS(strlen(keys[count]), key_size);
        TEST_ASSERT_EQUALS(0, memcmp(keys[count], key, key_size));
        ppdb_base_free(key);

        err = ppdb_iterator_value(iter, &value, &value_size);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        TEST_ASSERT_EQUALS(strlen(values[count]) + 1, value_size);
        TEST_ASSERT_EQUALS(0, memcmp(values[count], value, value_size));
        ppdb_base_free(value);

        err = ppdb_iterator_next(iter);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        count++;
    }
    TEST_ASSERT_EQUALS(3, count);

    // Seek to middle
    err = ppdb_iterator_seek(iter, "key2", strlen("key2"));
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(true, ppdb_iterator_valid(iter));

    void* key;
    void* value;
    size_t key_size, value_size;
    err = ppdb_iterator_key(iter, &key, &key_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen("key2"), key_size);
    TEST_ASSERT_EQUALS(0, memcmp("key2", key, key_size));
    ppdb_base_free(key);

    err = ppdb_iterator_value(iter, &value, &value_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen("value2") + 1, value_size);
    TEST_ASSERT_EQUALS(0, memcmp("value2", value, value_size));
    ppdb_base_free(value);

    // Cleanup iterator
    err = ppdb_iterator_destroy(iter);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Commit transaction
    err = ppdb_txn_commit(txn);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: index_iterator\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_index_basic);
    TEST_RUN(test_index_lookup);
    TEST_RUN(test_index_iterator);
    
    TEST_SUMMARY();
    return TEST_RESULT();
}