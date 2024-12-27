#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/wal.h>
#include <ppdb/error.h>

// 声明WAL测试套件
extern test_suite_t wal_suite;

int main(int argc, char* argv[]) {
    // 初始化日志系统
    ppdb_log_init(PPDB_LOG_LEVEL_DEBUG);
    ppdb_log_info("Starting WAL tests...");

    // 运行WAL测试套件
    int result = run_test_suite(&wal_suite);

    ppdb_log_info("WAL tests completed with result: %d", result);
    return result;
} 