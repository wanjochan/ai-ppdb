/*
 * test_database.c - Database Layer Tests
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

    return PPDB_OK;
}

// Test database initialization
static int test_database_init(void) {
    printf("\n=== Starting test: database_init ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Test normal initialization
    err = ppdb_database_init(&db, &config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(db);

    // Test initialization with NULL parameters
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_START, ppdb_database_init(NULL, &config));
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_START, ppdb_database_init(&db, NULL));

    cleanup_resources();
    printf("Test passed: database_init\n");
    return 0;
}

// Test basic transaction operations
static int test_database_transaction(void) {
    printf("\n=== Starting test: database_transaction ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Initialize database
    err = ppdb_database_init(&db, &config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Begin transaction
    ppdb_txn_t* txn = NULL;
    err = ppdb_txn_begin(db, &txn, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_NOT_NULL(txn);

    // Put some data
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_put(txn, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Get the data back
    void* result;
    size_t result_size;
    err = ppdb_get(txn, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen(value) + 1, result_size);
    TEST_ASSERT_EQUALS(0, memcmp(value, result, result_size));
    ppdb_base_free(result);

    // Delete the data
    err = ppdb_delete(txn, key, strlen(key));
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Verify deletion
    err = ppdb_get(txn, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_NOT_FOUND, err);

    // Commit transaction
    err = ppdb_txn_commit(txn);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: database_transaction\n");
    return 0;
}

// Test database statistics
static int test_database_stats(void) {
    printf("\n=== Starting test: database_stats ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Initialize database
    err = ppdb_database_init(&db, &config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Get initial stats
    ppdb_database_stats_t stats;
    err = ppdb_database_get_stats(db, &stats);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(0, stats.total_txns);
    TEST_ASSERT_EQUALS(0, stats.committed_txns);
    TEST_ASSERT_EQUALS(0, stats.aborted_txns);
    TEST_ASSERT_EQUALS(0, stats.conflicts);
    TEST_ASSERT_EQUALS(0, stats.deadlocks);
    TEST_ASSERT_EQUALS(0, stats.cache_hits);
    TEST_ASSERT_EQUALS(0, stats.cache_misses);
    TEST_ASSERT_EQUALS(0, stats.bytes_written);
    TEST_ASSERT_EQUALS(0, stats.bytes_read);

    // Do some operations to affect stats
    ppdb_txn_t* txn = NULL;
    err = ppdb_txn_begin(db, &txn, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_put(txn, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    err = ppdb_txn_commit(txn);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Check updated stats
    err = ppdb_database_get_stats(db, &stats);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(1, stats.total_txns);
    TEST_ASSERT_EQUALS(1, stats.committed_txns);
    TEST_ASSERT_GREATER_THAN(0, stats.bytes_written);

    cleanup_resources();
    printf("Test passed: database_stats\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_database_init);
    TEST_RUN(test_database_transaction);
    TEST_RUN(test_database_stats);
    
    TEST_SUMMARY();
    return TEST_RESULT();
}