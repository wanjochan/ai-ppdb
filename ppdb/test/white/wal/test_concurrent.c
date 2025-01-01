#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "ppdb/ppdb_logger.h"
#include "kvstore/internal/kvstore_fs.h"

#define NUM_THREADS 4
#define NUM_OPERATIONS 1000

// Thread argument structure
typedef struct {
    ppdb_wal_t* wal;
    int thread_id;
} thread_args_t;

// Worker thread function for concurrent writes
static void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    ppdb_wal_t* wal = args->wal;
    int thread_id = args->thread_id;
    
    char key_buf[32];
    char value_buf[32];
    
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        // Generate key-value pair
        snprintf(key_buf, sizeof(key_buf), "key_%d_%d", thread_id, i);
        snprintf(value_buf, sizeof(value_buf), "value_%d_%d", thread_id, i);
        
        // Write to WAL
        ppdb_error_t err = ppdb_wal_write(wal, PPDB_WAL_PUT,
            (uint8_t*)key_buf, strlen(key_buf) + 1,
            (uint8_t*)value_buf, strlen(value_buf) + 1);
        assert(err == PPDB_OK);
        
        // Randomly delete some keys
        if (i % 3 == 0) {
            err = ppdb_wal_write(wal, PPDB_WAL_DELETE,
                (uint8_t*)key_buf, strlen(key_buf) + 1,
                NULL, 0);
            assert(err == PPDB_OK);
        }
    }
    
    return NULL;
}

// Test concurrent WAL write operations
static int test_wal_concurrent_write(void) {
    ppdb_log_info("Testing concurrent WAL write operations...");
    
    const char* test_dir = "test_wal_concurrent.db";
    cleanup_test_dir(test_dir);
    
    // Initialize WAL
    ppdb_wal_config_t config = {
        .dir_path = {0},
        .segment_size = 4096,  // Small segment size to trigger frequent switches
        .sync_write = true
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_wal_t* wal = NULL;
    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
    
    // Create and start threads
    pthread_t threads[NUM_THREADS];
    thread_args_t thread_args[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].wal = wal;
        thread_args[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Verify data through recovery
    ppdb_memtable_t* table = NULL;
    err = ppdb_memtable_create(1024 * 1024, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create memtable for recovery");
    
    err = ppdb_wal_recover(wal, table);
    TEST_ASSERT(err == PPDB_OK, "Failed to recover WAL");
    
    // Verify the final state
    char key_buf[32];
    char value_buf[32];
    uint8_t read_buf[32];
    size_t read_len;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < NUM_OPERATIONS; i++) {
            snprintf(key_buf, sizeof(key_buf), "key_%d_%d", t, i);
            snprintf(value_buf, sizeof(value_buf), "value_%d_%d", t, i);
            
            read_len = sizeof(read_buf);
            err = ppdb_memtable_get(table, (uint8_t*)key_buf, strlen(key_buf) + 1,
                                   read_buf, &read_len);
                                   
            if (i % 3 == 0) {
                // Key should be deleted
                TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should be deleted");
            } else {
                // Key should exist with correct value
                TEST_ASSERT(err == PPDB_OK, "Key should exist");
                TEST_ASSERT(read_len == strlen(value_buf) + 1, "Value length mismatch");
                TEST_ASSERT(memcmp(read_buf, value_buf, read_len) == 0, "Value content mismatch");
            }
        }
    }
    
    // Cleanup
    ppdb_memtable_destroy(table);
    ppdb_wal_destroy(wal);
    cleanup_test_dir(test_dir);
    
    return 0;
}

// Test concurrent WAL write with archiving
static int test_wal_concurrent_write_archive(void) {
    ppdb_log_info("Testing concurrent WAL write with archiving...");
    
    const char* test_dir = "test_wal_concurrent_archive.db";
    cleanup_test_dir(test_dir);
    
    // Initialize WAL with small segment size
    ppdb_wal_config_t config = {
        .dir_path = {0},
        .segment_size = 1024,  // Very small segment size to trigger frequent archiving
        .sync_write = true
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_wal_t* wal = NULL;
    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
    
    // Create and start writer threads
    pthread_t threads[NUM_THREADS];
    thread_args_t thread_args[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].wal = wal;
        thread_args[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]);
    }
    
    // Perform periodic archiving while threads are writing
    for (int i = 0; i < 5; i++) {
        usleep(100000);  // Sleep for 100ms
        err = ppdb_wal_archive(wal);
        TEST_ASSERT(err == PPDB_OK, "Archive operation failed");
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Final archive
    err = ppdb_wal_archive(wal);
    TEST_ASSERT(err == PPDB_OK, "Final archive operation failed");
    
    // Verify data through recovery
    ppdb_memtable_t* table = NULL;
    err = ppdb_memtable_create(1024 * 1024, &table);
    TEST_ASSERT(err == PPDB_OK, "Failed to create memtable for recovery");
    
    err = ppdb_wal_recover(wal, table);
    TEST_ASSERT(err == PPDB_OK, "Failed to recover WAL");
    
    // Verify the final state
    char key_buf[32];
    char value_buf[32];
    uint8_t read_buf[32];
    size_t read_len;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < NUM_OPERATIONS; i++) {
            snprintf(key_buf, sizeof(key_buf), "key_%d_%d", t, i);
            snprintf(value_buf, sizeof(value_buf), "value_%d_%d", t, i);
            
            read_len = sizeof(read_buf);
            err = ppdb_memtable_get(table, (uint8_t*)key_buf, strlen(key_buf) + 1,
                                   read_buf, &read_len);
                                   
            if (i % 3 == 0) {
                // Key should be deleted
                TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should be deleted");
            } else {
                // Key should exist with correct value
                TEST_ASSERT(err == PPDB_OK, "Key should exist");
                TEST_ASSERT(read_len == strlen(value_buf) + 1, "Value length mismatch");
                TEST_ASSERT(memcmp(read_buf, value_buf, read_len) == 0, "Value content mismatch");
            }
        }
    }
    
    // Cleanup
    ppdb_memtable_destroy(table);
    ppdb_wal_destroy(wal);
    cleanup_test_dir(test_dir);
    
    return 0;
}

// Register WAL concurrent tests
void register_wal_concurrent_tests(void) {
    TEST_REGISTER(test_wal_concurrent_write);
    TEST_REGISTER(test_wal_concurrent_write_archive);
} 