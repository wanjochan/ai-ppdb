#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/kvstore.h>
#include <ppdb/error.h>
#include <ppdb/logger.h>

// Forward declaration of worker thread function
static void* concurrent_worker(void* arg);

// Test KVStore create/close
static int test_kvstore_create_close(void) {
    ppdb_log_info("Testing KVStore create/close...");
    
    const char* test_dir = "test_kvstore_create.db";
    cleanup_test_dir(test_dir);
    
    // Create KVStore
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 4096,
        .l0_size = 4096 * 4,
        .l0_files = 4,
        .compression = PPDB_COMPRESSION_NONE
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
    TEST_ASSERT(store != NULL, "KVStore pointer is NULL");
    
    // Close KVStore
    ppdb_kvstore_close(store);
    
    cleanup_test_dir(test_dir);
    return 0;
}

// Test KVStore basic operations
static int test_kvstore_basic_ops(void) {
    ppdb_log_info("Testing KVStore basic operations...");
    
    const char* test_dir = "test_kvstore_basic.db";
    cleanup_test_dir(test_dir);
    
    // Create KVStore
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 4096,
        .l0_size = 4096 * 4,
        .l0_files = 4,
        .compression = PPDB_COMPRESSION_NONE
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
    
    // Put key-value pair
    const uint8_t* key = (const uint8_t*)"test_key";
    const uint8_t* value = (const uint8_t*)"test_value";
    err = ppdb_kvstore_put(store, key, strlen((const char*)key), value, strlen((const char*)value));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    
    // Get value
    uint8_t buf[256] = {0};
    size_t size = sizeof(buf);
    err = ppdb_kvstore_get(store, key, strlen((const char*)key), buf, &size);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    TEST_ASSERT(size == strlen((const char*)value), "Value size mismatch");
    TEST_ASSERT(memcmp(buf, value, size) == 0, "Value content mismatch");
    
    // Delete key
    err = ppdb_kvstore_delete(store, key, strlen((const char*)key));
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key");
    
    // Try to get deleted key
    size = sizeof(buf);
    err = ppdb_kvstore_get(store, key, strlen((const char*)key), buf, &size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist after deletion");
    
    // Close KVStore
    ppdb_kvstore_close(store);
    
    cleanup_test_dir(test_dir);
    return 0;
}

// Test KVStore recovery
static int test_kvstore_recovery(void) {
    ppdb_log_info("Testing KVStore recovery...");
    
    const char* test_dir = "test_kvstore_recovery.db";
    cleanup_test_dir(test_dir);
    
    // First session: write data
    {
        ppdb_kvstore_config_t config = {
            .dir_path = {0},
            .memtable_size = 4096,
            .l0_size = 4096 * 4,
            .l0_files = 4,
            .compression = PPDB_COMPRESSION_NONE
        };
        strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
        
        ppdb_kvstore_t* store = NULL;
        ppdb_error_t err = ppdb_kvstore_create(&config, &store);
        TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
        
        const uint8_t* key = (const uint8_t*)"recovery_key";
        const uint8_t* value = (const uint8_t*)"recovery_value";
        err = ppdb_kvstore_put(store, key, strlen((const char*)key), value, strlen((const char*)value));
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
        
        ppdb_kvstore_close(store);
    }
    
    // Second session: recover data
    {
        ppdb_kvstore_config_t config = {
            .dir_path = {0},
            .memtable_size = 4096,
            .l0_size = 4096 * 4,
            .l0_files = 4,
            .compression = PPDB_COMPRESSION_NONE
        };
        strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
        
        ppdb_kvstore_t* store = NULL;
        ppdb_error_t err = ppdb_kvstore_create(&config, &store);
        TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
        
        // Verify recovered data
        const uint8_t* key = (const uint8_t*)"recovery_key";
        uint8_t buf[256] = {0};
        size_t size = sizeof(buf);
        err = ppdb_kvstore_get(store, key, strlen((const char*)key), buf, &size);
        TEST_ASSERT(err == PPDB_OK, "Failed to get value");
        TEST_ASSERT(strcmp((const char*)buf, "recovery_value") == 0, "Recovered value does not match");
        
        ppdb_kvstore_close(store);
    }
    
    cleanup_test_dir(test_dir);
    return 0;
}

// Thread data structure
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    int success_count;
} thread_data_t;

// Worker thread function
static void* concurrent_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    ppdb_kvstore_t* store = data->store;
    int thread_id = data->thread_id;
    
    char key[32];
    char value[32];
    char buf[32];
    
    for (int i = 0; i < 100; i++) {
        // Put key-value pair
        snprintf(key, sizeof(key), "key_%d_%d", thread_id, i);
        snprintf(value, sizeof(value), "value_%d_%d", thread_id, i);
        
        ppdb_error_t err = ppdb_kvstore_put(store, 
                                          (const uint8_t*)key, strlen(key),
                                          (const uint8_t*)value, strlen(value));
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d failed to put key %s: %d", thread_id, key, err);
            continue;
        }
        
        // Get value back
        size_t size = sizeof(buf);
        err = ppdb_kvstore_get(store, (const uint8_t*)key, strlen(key), (uint8_t*)buf, &size);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d failed to get key %s: %d", thread_id, key, err);
            continue;
        }
        
        if (strcmp(buf, value) == 0) {
            data->success_count++;
        }
    }
    
    return NULL;
}

// Test KVStore concurrent operations
static int test_kvstore_concurrent(void) {
    ppdb_log_info("Testing KVStore concurrent operations...");
    
    const char* test_dir = "test_kvstore_concurrent.db";
    cleanup_test_dir(test_dir);
    
    // Create KVStore
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 4096 * 16,  // Larger size for concurrent test
        .l0_size = 4096 * 64,
        .l0_files = 8,
        .compression = PPDB_COMPRESSION_NONE
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Failed to create KVStore");
    
    // Create threads
    const int num_threads = 4;
    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].store = store;
        thread_data[i].thread_id = i;
        thread_data[i].success_count = 0;
        pthread_create(&threads[i], NULL, concurrent_worker, &thread_data[i]);
    }
    
    // Wait for threads to complete
    int total_success = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_success += thread_data[i].success_count;
    }
    
    // Verify results
    ppdb_log_info("Total successful operations: %d", total_success);
    TEST_ASSERT(total_success > 0, "No successful operations");
    
    // Verify some keys
    for (int i = 0; i < num_threads; i++) {
        for (int j = 0; j < 100; j++) {
            char key[32];
            char expected_value[32];
            uint8_t buf[32];
            size_t size = sizeof(buf);
            
            snprintf(key, sizeof(key), "key_%d_%d", i, j);
            snprintf(expected_value, sizeof(expected_value), "value_%d_%d", i, j);
            
            err = ppdb_kvstore_get(store, (const uint8_t*)key, strlen(key), buf, &size);
            if (err == PPDB_OK) {
                TEST_ASSERT(strcmp((const char*)buf, expected_value) == 0, "Value mismatch");
            }
        }
    }
    
    // Close KVStore
    ppdb_kvstore_close(store);
    
    cleanup_test_dir(test_dir);
    return 0;
}

// KVStore test suite definition
static const test_case_t kvstore_test_cases[] = {
    {"create_close", test_kvstore_create_close},
    {"basic_ops", test_kvstore_basic_ops},
    {"recovery", test_kvstore_recovery},
    {"concurrent", test_kvstore_concurrent}
};

// Export KVStore test suite
const test_suite_t kvstore_suite = {
    .name = "KVStore",
    .cases = kvstore_test_cases,
    .num_cases = sizeof(kvstore_test_cases) / sizeof(kvstore_test_cases[0])
};