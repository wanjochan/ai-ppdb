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

// Test concurrent reads and writes
static int test_mvcc_concurrent_rw(void) {
    printf("\n=== Starting test: mvcc_concurrent_rw ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    const char* key = "test_key";
    const char* value1 = "value1";
    const char* value2 = "value2";

    // Start writer transaction
    ppdb_txn_t* writer = NULL;
    err = ppdb_txn_begin(db, &writer, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Write initial value
    err = ppdb_put(writer, key, strlen(key), value1, strlen(value1) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Start reader transaction
    ppdb_txn_t* reader = NULL;
    err = ppdb_txn_begin(db, &reader, PPDB_TXN_READONLY);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Reader should see the uncommitted value in READ_UNCOMMITTED
    err = ppdb_txn_set_isolation(reader, PPDB_TXN_READ_UNCOMMITTED);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    void* result;
    size_t result_size;
    err = ppdb_get(reader, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen(value1) + 1, result_size);
    TEST_ASSERT_EQUALS(0, memcmp(value1, result, result_size));
    ppdb_base_free(result);

    // Writer updates value
    err = ppdb_put(writer, key, strlen(key), value2, strlen(value2) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Reader in READ_COMMITTED should not see the change
    err = ppdb_txn_set_isolation(reader, PPDB_TXN_READ_COMMITTED);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    err = ppdb_get(reader, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_NOT_FOUND, err);

    // Commit writer
    err = ppdb_txn_commit(writer);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Reader in READ_COMMITTED should now see the new value
    err = ppdb_get(reader, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen(value2) + 1, result_size);
    TEST_ASSERT_EQUALS(0, memcmp(value2, result, result_size));
    ppdb_base_free(result);

    // Cleanup reader
    err = ppdb_txn_commit(reader);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: mvcc_concurrent_rw\n");
    return 0;
}

// Test snapshot isolation
static int test_mvcc_snapshot(void) {
    printf("\n=== Starting test: mvcc_snapshot ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    const char* key = "test_key";
    const char* value1 = "value1";
    const char* value2 = "value2";

    // Create initial value
    {
        ppdb_txn_t* txn = NULL;
        err = ppdb_txn_begin(db, &txn, 0);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_put(txn, key, strlen(key), value1, strlen(value1) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_txn_commit(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    // Start snapshot reader
    ppdb_txn_t* reader = NULL;
    err = ppdb_txn_begin(db, &reader, PPDB_TXN_READONLY);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    err = ppdb_txn_set_isolation(reader, PPDB_TXN_REPEATABLE_READ);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Verify initial value
    void* result;
    size_t result_size;
    err = ppdb_get(reader, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen(value1) + 1, result_size);
    TEST_ASSERT_EQUALS(0, memcmp(value1, result, result_size));
    ppdb_base_free(result);

    // Update value in another transaction
    {
        ppdb_txn_t* writer = NULL;
        err = ppdb_txn_begin(db, &writer, 0);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_put(writer, key, strlen(key), value2, strlen(value2) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_txn_commit(writer);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    // Reader should still see old value
    err = ppdb_get(reader, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen(value1) + 1, result_size);
    TEST_ASSERT_EQUALS(0, memcmp(value1, result, result_size));
    ppdb_base_free(result);

    // Cleanup reader
    err = ppdb_txn_commit(reader);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: mvcc_snapshot\n");
    return 0;
}

// Test write skew anomaly prevention
static int test_mvcc_write_skew(void) {
    printf("\n=== Starting test: mvcc_write_skew ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    const char* key1 = "balance1";
    const char* key2 = "balance2";
    const char* value = "500";

    // Initialize balances
    {
        ppdb_txn_t* txn = NULL;
        err = ppdb_txn_begin(db, &txn, 0);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_put(txn, key1, strlen(key1), value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_put(txn, key2, strlen(key2), value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_txn_commit(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    // Start two concurrent transactions
    ppdb_txn_t* txn1 = NULL;
    ppdb_txn_t* txn2 = NULL;

    err = ppdb_txn_begin(db, &txn1, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    err = ppdb_txn_set_isolation(txn1, PPDB_TXN_SERIALIZABLE);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    err = ppdb_txn_begin(db, &txn2, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    err = ppdb_txn_set_isolation(txn2, PPDB_TXN_SERIALIZABLE);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Both transactions read both balances
    void* result1;
    void* result2;
    size_t size1, size2;

    err = ppdb_get(txn1, key1, strlen(key1), &result1, &size1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    err = ppdb_get(txn1, key2, strlen(key2), &result2, &size2);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    err = ppdb_get(txn2, key1, strlen(key1), &result1, &size1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    err = ppdb_get(txn2, key2, strlen(key2), &result2, &size2);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Both try to withdraw from different accounts
    const char* new_value = "0";
    err = ppdb_put(txn1, key1, strlen(key1), new_value, strlen(new_value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    err = ppdb_put(txn2, key2, strlen(key2), new_value, strlen(new_value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // First commit succeeds
    err = ppdb_txn_commit(txn1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Second commit should fail due to serialization conflict
    err = ppdb_txn_commit(txn2);
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_CONFLICT, err);

    cleanup_resources();
    printf("Test passed: mvcc_write_skew\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_mvcc_concurrent_rw);
    TEST_RUN(test_mvcc_snapshot);
    TEST_RUN(test_mvcc_write_skew);
    
    TEST_SUMMARY();
    return TEST_RESULT();
}