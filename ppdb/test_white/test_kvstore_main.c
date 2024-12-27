#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/kvstore.h>
#include <ppdb/error.h>

// 声明KVStore测试套件
extern test_suite_t kvstore_suite;

int main(int argc, char* argv[]) {
    // 初始化日志系统
    ppdb_log_init(PPDB_LOG_LEVEL_DEBUG);
    ppdb_log_info("Starting KVStore tests...");

    // 运行KVStore测试套件
    int result = run_test_suite(&kvstore_suite);

    ppdb_log_info("KVStore tests completed with result: %d", result);
    return result;
} 