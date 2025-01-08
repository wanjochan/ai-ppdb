/*
 * test_database_txn.c - Database Transaction Tests
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

// Test transaction isolation levels
static int test_txn_isolation(void) {
    printf("\n=== Starting test: txn_isolation ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Test each isolation level
    ppdb_txn_isolation_t levels[] = {
        PPDB_TXN_READ_UNCOMMITTED,
        PPDB_TXN_READ_COMMITTED,
        PPDB_TXN_REPEATABLE_READ,
        PPDB_TXN_SERIALIZABLE
    };

    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
        ppdb_txn_t* txn = NULL;
        err = ppdb_txn_begin(db, &txn, 0);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        // Set isolation level
        err = ppdb_txn_set_isolation(txn, levels[i]);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        // Verify isolation level
        ppdb_txn_isolation_t current;
        err = ppdb_txn_get_isolation(txn, &current);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        TEST_ASSERT_EQUALS(levels[i], current);

        err = ppdb_txn_commit(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    cleanup_resources();
    printf("Test passed: txn_isolation\n");
    return 0;
}

// Test transaction commit and rollback
static int test_txn_commit_rollback(void) {
    printf("\n=== Starting test: txn_commit_rollback ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    const char* key = "test_key";
    const char* value = "test_value";

    // Test commit
    {
        ppdb_txn_t* txn = NULL;
        err = ppdb_txn_begin(db, &txn, 0);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_put(txn, key, strlen(key), value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_txn_commit(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        // Verify data persists after commit
        txn = NULL;
        err = ppdb_txn_begin(db, &txn, 0);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        void* result;
        size_t result_size;
        err = ppdb_get(txn, key, strlen(key), &result, &result_size);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        TEST_ASSERT_EQUALS(strlen(value) + 1, result_size);
        TEST_ASSERT_EQUALS(0, memcmp(value, result, result_size));
        ppdb_base_free(result);

        err = ppdb_txn_commit(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    // Test rollback
    {
        ppdb_txn_t* txn = NULL;
        err = ppdb_txn_begin(db, &txn, 0);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        const char* new_value = "new_value";
        err = ppdb_put(txn, key, strlen(key), new_value, strlen(new_value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_txn_abort(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        // Verify data remains unchanged after rollback
        txn = NULL;
        err = ppdb_txn_begin(db, &txn, 0);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        void* result;
        size_t result_size;
        err = ppdb_get(txn, key, strlen(key), &result, &result_size);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
        TEST_ASSERT_EQUALS(strlen(value) + 1, result_size);
        TEST_ASSERT_EQUALS(0, memcmp(value, result, result_size));
        ppdb_base_free(result);

        err = ppdb_txn_commit(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    cleanup_resources();
    printf("Test passed: txn_commit_rollback\n");
    return 0;
}

// Test transaction flags
static int test_txn_flags(void) {
    printf("\n=== Starting test: txn_flags ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Test read-only transaction
    {
        ppdb_txn_t* txn = NULL;
        err = ppdb_txn_begin(db, &txn, PPDB_TXN_READONLY);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        const char* key = "test_key";
        const char* value = "test_value";
        err = ppdb_put(txn, key, strlen(key), value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_READONLY, err);

        err = ppdb_txn_abort(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    // Test sync transaction
    {
        ppdb_txn_t* txn = NULL;
        err = ppdb_txn_begin(db, &txn, PPDB_TXN_SYNC);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        const char* key = "test_key";
        const char* value = "test_value";
        err = ppdb_put(txn, key, strlen(key), value, strlen(value) + 1);
        TEST_ASSERT_EQUALS(PPDB_OK, err);

        err = ppdb_txn_commit(txn);
        TEST_ASSERT_EQUALS(PPDB_OK, err);
    }

    cleanup_resources();
    printf("Test passed: txn_flags\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_txn_isolation);
    TEST_RUN(test_txn_commit_rollback);
    TEST_RUN(test_txn_flags);
    
    TEST_SUMMARY();
    return TEST_RESULT();
}