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

// Test basic storage operations
static int test_memkv_basic(void) {
    printf("\n=== Starting test: memkv_basic ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Start transaction
    ppdb_txn_t* txn = NULL;
    err = ppdb_txn_begin(db, &txn, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Test put and get
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_put(txn, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    void* result;
    size_t result_size;
    err = ppdb_get(txn, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(strlen(value) + 1, result_size);
    TEST_ASSERT_EQUALS(0, memcmp(value, result, result_size));
    ppdb_base_free(result);

    // Test delete
    err = ppdb_delete(txn, key, strlen(key));
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    err = ppdb_get(txn, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_NOT_FOUND, err);

    // Commit transaction
    err = ppdb_txn_commit(txn);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: memkv_basic\n");
    return 0;
}

// Test memory limits
static int test_memkv_limits(void) {
    printf("\n=== Starting test: memkv_limits ===\n");
    
    // Initialize with small memory limit
    config.memory_limit = 1024;  // 1KB
    ppdb_error_t err = ppdb_database_init(&db, &config);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Start transaction
    ppdb_txn_t* txn = NULL;
    err = ppdb_txn_begin(db, &txn, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Try to store data larger than memory limit
    const char* key = "test_key";
    char* large_value = ppdb_base_malloc(2048);  // 2KB
    memset(large_value, 'x', 2047);
    large_value[2047] = '\0';

    err = ppdb_put(txn, key, strlen(key), large_value, 2048);
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_STORAGE, err);

    ppdb_base_free(large_value);

    // Abort transaction
    err = ppdb_txn_abort(txn);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: memkv_limits\n");
    return 0;
}

// Test cache operations
static int test_memkv_cache(void) {
    printf("\n=== Starting test: memkv_cache ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    // Start transaction
    ppdb_txn_t* txn = NULL;
    err = ppdb_txn_begin(db, &txn, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Get initial stats
    ppdb_database_stats_t stats;
    err = ppdb_database_get_stats(db, &stats);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    uint64_t initial_hits = stats.cache_hits;
    uint64_t initial_misses = stats.cache_misses;

    // Put some data
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_put(txn, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // First get should be a cache miss
    void* result;
    size_t result_size;
    err = ppdb_get(txn, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    ppdb_base_free(result);

    err = ppdb_database_get_stats(db, &stats);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(initial_misses + 1, stats.cache_misses);

    // Second get should be a cache hit
    err = ppdb_get(txn, key, strlen(key), &result, &result_size);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    ppdb_base_free(result);

    err = ppdb_database_get_stats(db, &stats);
    TEST_ASSERT_EQUALS(PPDB_OK, err);
    TEST_ASSERT_EQUALS(initial_hits + 1, stats.cache_hits);

    // Commit transaction
    err = ppdb_txn_commit(txn);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    cleanup_resources();
    printf("Test passed: memkv_cache\n");
    return 0;
}

// Test concurrent access
static int test_memkv_concurrent(void) {
    printf("\n=== Starting test: memkv_concurrent ===\n");
    
    ppdb_error_t err = test_setup();
    if (err != PPDB_OK) {
        printf("ERROR: Failed to setup test environment: %d\n", err);
        return -1;
    }

    const char* key = "test_key";
    const char* value1 = "value1";
    const char* value2 = "value2";

    // Start two transactions
    ppdb_txn_t* txn1 = NULL;
    ppdb_txn_t* txn2 = NULL;

    err = ppdb_txn_begin(db, &txn1, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    err = ppdb_txn_begin(db, &txn2, 0);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // First transaction writes
    err = ppdb_put(txn1, key, strlen(key), value1, strlen(value1) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Second transaction tries to write same key
    err = ppdb_put(txn2, key, strlen(key), value2, strlen(value2) + 1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // First transaction commits
    err = ppdb_txn_commit(txn1);
    TEST_ASSERT_EQUALS(PPDB_OK, err);

    // Second transaction should fail to commit
    err = ppdb_txn_commit(txn2);
    TEST_ASSERT_EQUALS(PPDB_DATABASE_ERR_CONFLICT, err);

    cleanup_resources();
    printf("Test passed: memkv_concurrent\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_memkv_basic);
    TEST_RUN(test_memkv_limits);
    TEST_RUN(test_memkv_cache);
    TEST_RUN(test_memkv_concurrent);
    
    TEST_SUMMARY();
    return TEST_RESULT();
}