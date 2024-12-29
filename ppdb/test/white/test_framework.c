#include "test_framework.h"
#include <ppdb/logger.h>
#include <ppdb/fs.h>

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
    if (!dir_path) return;

    // 最大重试次数
    const int max_retries = 3;
    int retry_count = 0;

    while (retry_count < max_retries) {
        // 如果目录存在，则删除
        if (ppdb_fs_dir_exists(dir_path)) {
            ppdb_error_t err = ppdb_remove_directory(dir_path);
            if (err == PPDB_OK) {
                break;  // 成功删除，退出循环
            }
            
            ppdb_log_warn("Failed to remove directory %s (attempt %d/%d): %s",
                         dir_path, retry_count + 1, max_retries,
                         ppdb_error_string(err));
            
            // 等待一段时间后重试
            usleep(500000);  // 500ms
            retry_count++;
        } else {
            break;  // 目录不存在，直接退出
        }
    }

    // 最后检查目录是否还存在
    if (retry_count == max_retries && ppdb_fs_dir_exists(dir_path)) {
        ppdb_log_error("Failed to remove directory %s after %d attempts",
                      dir_path, max_retries);
    }

    // 等待一段时间确保资源完全释放
    usleep(500000);  // 500ms
} 