#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/wal.h>
#include <ppdb/error.h>
#include <ppdb/logger.h>
#include "common/fs.h"

// Test WAL filesystem operations
static int test_wal_fs_ops(void) {
    ppdb_log_info("Testing WAL filesystem operations...");
    
    const char* test_dir = "test_wal_fs.db";
    cleanup_test_dir(test_dir);
    
    // Create directory
    ppdb_error_t err = ppdb_ensure_directory(test_dir);
    TEST_ASSERT(err == PPDB_OK, "Failed to create directory");
    
    // Initialize WAL
    ppdb_wal_config_t config = {
        .dir_path = {0}
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_wal_t* wal = NULL;
    err = ppdb_wal_create(&config, &wal);
    TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
    TEST_ASSERT(wal != NULL, "WAL pointer is NULL");
    
    // Close WAL
    ppdb_wal_destroy(wal);
    
    cleanup_test_dir(test_dir);
    return 0;
}

// Test WAL write operations
static int test_wal_write(void) {
    ppdb_log_info("Testing WAL write operations...");
    
    const char* test_dir = "test_wal_write.db";
    cleanup_test_dir(test_dir);
    
    // Initialize WAL
    ppdb_wal_config_t config = {
        .dir_path = {0}
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_wal_t* wal = NULL;
    ppdb_error_t err = ppdb_wal_create(&config, &wal);
    TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
    
    // Write data
    const char* key = "test_key";
    const char* value = "test_value";
    err = ppdb_wal_write(wal, PPDB_WAL_RECORD_PUT, key, strlen(key), value, strlen(value));
    TEST_ASSERT(err == PPDB_OK, "Failed to write to WAL");
    
    // Close WAL
    ppdb_wal_destroy(wal);
    
    cleanup_test_dir(test_dir);
    return 0;
}

// Test WAL recovery
static int test_wal_recovery(void) {
    ppdb_log_info("Testing WAL recovery...");
    
    const char* test_dir = "test_wal_recovery.db";
    cleanup_test_dir(test_dir);
    
    // Initialize WAL
    ppdb_wal_config_t config = {
        .dir_path = {0}
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    // First session: write data
    {
        ppdb_wal_t* wal = NULL;
        ppdb_error_t err = ppdb_wal_create(&config, &wal);
        TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
        
        const char* key = "recovery_key";
        const char* value = "recovery_value";
        err = ppdb_wal_write(wal, PPDB_WAL_RECORD_PUT, key, strlen(key), value, strlen(value));
        TEST_ASSERT(err == PPDB_OK, "Failed to write to WAL");
        
        ppdb_wal_close(wal);
    }
    
    // Second session: recover data
    {
        ppdb_wal_t* wal = NULL;
        ppdb_memtable_t* memtable = NULL;
        ppdb_error_t err;
        
        // Create memtable
        err = ppdb_memtable_create(4096, &memtable);
        TEST_ASSERT(err == PPDB_OK, "Failed to create memtable");
        
        // Open WAL and recover
        err = ppdb_wal_create(&config, &wal);
        TEST_ASSERT(err == PPDB_OK, "Failed to create WAL");
        
        err = ppdb_wal_recover(wal, memtable);
        TEST_ASSERT(err == PPDB_OK, "Failed to recover from WAL");
        
        // Verify recovered data
        const char* key = "recovery_key";
        uint8_t recovered_value[256] = {0};
        size_t value_size = sizeof(recovered_value);
        err = ppdb_memtable_get(memtable, (const uint8_t*)key, strlen(key), recovered_value, &value_size);
        TEST_ASSERT(err == PPDB_OK, "Failed to get value from memtable");
        TEST_ASSERT(strcmp((const char*)recovered_value, "recovery_value") == 0, "Recovered value does not match");
        
        // Cleanup
        ppdb_wal_destroy(wal);
        ppdb_memtable_destroy(memtable);
    }
    
    cleanup_test_dir(test_dir);
    return 0;
}

// WAL test suite definition
static const test_case_t wal_test_cases[] = {
    {"fs_ops", test_wal_fs_ops},
    {"write", test_wal_write},
    {"recovery", test_wal_recovery}
};

// Export WAL test suite
const test_suite_t wal_suite = {
    .name = "WAL",
    .cases = wal_test_cases,
    .num_cases = sizeof(wal_test_cases) / sizeof(wal_test_cases[0])
}; 