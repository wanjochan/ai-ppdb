#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/memtable.h>
#include <ppdb/error.h>

// 声明MemTable测试套件
extern test_suite_t memtable_suite;

int main(int argc, char* argv[]) {
    // 初始化日志系统
    ppdb_log_init(PPDB_LOG_LEVEL_DEBUG);
    ppdb_log_info("Starting MemTable tests...");

    // 运行MemTable测试套件
    int result = run_test_suite(&memtable_suite);

    ppdb_log_info("MemTable tests completed with result: %d", result);
    return result;
} 