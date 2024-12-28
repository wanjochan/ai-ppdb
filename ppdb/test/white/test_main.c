#include "test_framework.h"

// 声明外部测试套件
extern test_suite_t kvstore_suite;
extern test_suite_t memtable_suite;
extern test_suite_t wal_suite;

int main(void) {
    // 初始化日志
    ppdb_log_config_t log_config = {
        .enabled = true,
        .outputs = PPDB_LOG_CONSOLE,
        .types = PPDB_LOG_TYPE_ALL,
        .async_mode = false,
        .buffer_size = 4096,
        .log_file = NULL,
        .level = PPDB_LOG_DEBUG
    };
    ppdb_log_init(&log_config);

    ppdb_log_info("Running all tests...");

    int failed = 0;
    failed += run_test_suite(&kvstore_suite);
    failed += run_test_suite(&memtable_suite);
    failed += run_test_suite(&wal_suite);

    if (failed > 0) {
        ppdb_log_error("Tests completed: %d suite(s) failed", failed);
    } else {
        ppdb_log_info("All test suites passed!");
    }

    ppdb_log_shutdown();
    return failed;
}