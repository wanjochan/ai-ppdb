#include "test_framework.h"
#include <ppdb/logger.h>
#include "common/fs.h"

int run_test_suite(const test_suite_t* suite) {
    ppdb_log_info("Running test suite: %s", suite->name);
    int failed_count = 0;
    
    for (size_t i = 0; i < suite->num_cases; i++) {
        ppdb_log_info("  Running test: %s", suite->cases[i].name);
        int result = suite->cases[i].fn();
        if (result != 0) {
            ppdb_log_error("  Test failed: %s (result = %d)", suite->cases[i].name, result);
            failed_count++;
        } else {
            ppdb_log_info("  Test passed: %s", suite->cases[i].name);
        }
    }
    
    if (failed_count > 0) {
        ppdb_log_error("Test suite %s completed with %d failures", suite->name, failed_count);
    } else {
        ppdb_log_info("Test suite %s completed successfully", suite->name);
    }
    
    return failed_count;
}

void cleanup_test_dir(const char* dir_path) {
    ppdb_error_t err = ppdb_remove_directory(dir_path);
    if (err != PPDB_OK) {
        ppdb_log_warn("Failed to remove directory %s: %d", dir_path, err);
    }
} 