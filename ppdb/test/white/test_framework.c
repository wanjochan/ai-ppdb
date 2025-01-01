#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_logger.h"

// 全局测试状态变量定义
test_state_t g_test_state = {0};
char current_test_name[256] = {0};
char current_test_result[32] = {0};
int test_case_count = 0;
int test_case_failed = 0;

// 段错误处理函数
static void segv_handler(int signo) {
    (void)signo;
    PPDB_LOG_ERROR("Segmentation fault in test: %s", current_test_name);
    exit(1);
}

// 超时处理函数
static void timeout_handler(int signo) {
    (void)signo;
    PPDB_LOG_ERROR("Test timeout: %s", current_test_name);
    exit(1);
}

// 初始化测试框架
void test_framework_init(void) {
    if (g_test_state.initialized) {
        return;
    }
    
    // 初始化日志系统
    ppdb_log_config_t log_config = {
        .enabled = true,
        .level = PPDB_LOG_DEBUG,
        .outputs = PPDB_LOG_CONSOLE,
        .types = PPDB_LOG_TYPE_ALL,
        .log_file = NULL,
        .async_mode = false,
        .buffer_size = 4096
    };
    ppdb_log_init(&log_config);
    
    // 设置信号处理
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = timeout_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGALRM, &sa, NULL) != 0) {
        ppdb_log_error("Failed to set SIGALRM handler");
        return;
    }
    
    // 设置段错误处理
    sa.sa_handler = segv_handler;
    if (sigaction(SIGSEGV, &sa, NULL) != 0) {
        ppdb_log_error("Failed to set SIGSEGV handler");
        return;
    }
    
    // 设置默认测试类型
    char* test_type_env = getenv("TEST_TYPE");
    if (test_type_env) {
        if (strcmp(test_type_env, "unit") == 0) {
            g_test_state.config.type = TEST_TYPE_UNIT;
        } else if (strcmp(test_type_env, "perf") == 0) {
            g_test_state.config.type = TEST_TYPE_PERF;
        } else if (strcmp(test_type_env, "stress") == 0) {
            g_test_state.config.type = TEST_TYPE_STRESS;
        } else {
            g_test_state.config.type = TEST_TYPE_ALL;
        }
    } else {
        g_test_state.config.type = TEST_TYPE_ALL;
    }
    
    g_test_state.initialized = true;
    g_test_state.stats.start_time = clock();
}

// 清理测试框架
void test_framework_cleanup(void) {
    if (!g_test_state.initialized) {
        return;
    }
    
    g_test_state.stats.end_time = clock();
    test_print_stats();
    
    g_test_state.initialized = false;
}

// 运行单个测试用例
int run_test_case(const test_case_t* test_case) {
    if (!test_case) {
        return -1;
    }
    
    // 设置当前测试名称
    strncpy(current_test_name, test_case->name, sizeof(current_test_name) - 1);
    current_test_name[sizeof(current_test_name) - 1] = '\0';
    
    // 打印测试信息
    ppdb_log_info("Running test: %s", test_case->name);
    ppdb_log_info("Description: %s", test_case->description);
    
    // 设置超时处理
    if (setjmp(g_test_state.timeout_jmp) == 0) {
        alarm(test_case->timeout_seconds);
        
        // 运行测试
        test_case_count++;
        int result = test_case->fn();
        
        // 取消超时
        alarm(0);
        
        // 处理结果
        if (result != 0) {
            test_case_failed++;
            strncpy(current_test_result, "FAILED", sizeof(current_test_result) - 1);
            return -1;
        } else {
            strncpy(current_test_result, "PASSED", sizeof(current_test_result) - 1);
            return 0;
        }
    } else {
        // 超时处理
        test_case_failed++;
        strncpy(current_test_result, "TIMEOUT", sizeof(current_test_result) - 1);
        return -1;
    }
}

// 运行测试套件
int run_test_suite(const test_suite_t* suite) {
    if (!suite) {
        return -1;
    }
    
    // 打印套件信息
    ppdb_log_info("Running test suite: %s", suite->name);
    
    // 运行套件初始化
    if (suite->setup) {
        suite->setup();
    }
    
    // 运行所有测试用例
    int failed = 0;
    for (size_t i = 0; i < suite->num_cases; i++) {
        const test_case_t* test_case = &suite->cases[i];
        if (test_case->skip) {
            ppdb_log_info("Skipping test: %s", test_case->name);
            continue;
        }
        
        if (run_test_case(test_case) != 0) {
            failed++;
        }
    }
    
    // 运行套件清理
    if (suite->teardown) {
        suite->teardown();
    }
    
    // 打印测试结果
    ppdb_log_info("Test Results:");
    ppdb_log_info("  Total Cases: %d", test_case_count);
    ppdb_log_info("  Failed: %d", test_case_failed);
    ppdb_log_info("  Duration: %.2f seconds", (double)(clock() - g_test_state.stats.start_time) / CLOCKS_PER_SEC);
    ppdb_log_info("  Peak Memory: %zu bytes", g_test_state.stats.peak_memory);
    
    return failed;
}

// 检查是否应该运行某个测试
bool test_framework_should_run(test_type_t type) {
    return g_test_state.config.type == TEST_TYPE_ALL || g_test_state.config.type == type;
}

// 打印测试统计信息
void test_print_stats(void) {
    double duration = (double)(g_test_state.stats.end_time - g_test_state.stats.start_time) / CLOCKS_PER_SEC;
    
    ppdb_log_info("Test Results:");
    ppdb_log_info("  Total Cases: %d", test_case_count);
    ppdb_log_info("  Failed: %d", test_case_failed);
    ppdb_log_info("  Duration: %.2f seconds", duration);
    ppdb_log_info("  Peak Memory: %zu bytes", g_test_state.stats.peak_memory);
}

// 获取测试结果
int test_get_result(void) {
    ppdb_log_info("Test Results:");
    ppdb_log_info("  Total Cases: %d", test_case_count);
    ppdb_log_info("  Failed: %d", test_case_failed);
    ppdb_log_info("  Duration: %.2f seconds", (double)(clock() - g_test_state.stats.start_time) / CLOCKS_PER_SEC);
    ppdb_log_info("  Peak Memory: %zu bytes", g_test_state.stats.peak_memory);
    
    return test_case_failed;
}