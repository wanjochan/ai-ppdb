#include "test_framework.h"

// 全局变量定义
test_state_t g_test_state;
char current_test_name[256];
char current_test_result[32];
int test_case_count = 0;
int test_case_failed = 0;

// 信号处理函数
static void test_timeout_handler(int sig) {
    (void)sig;
    longjmp(g_test_state.timeout_jmp, 1);
}

// 初始化测试框架
void test_framework_init(void) {
    memset(&g_test_state, 0, sizeof(test_state_t));
    g_test_state.initialized = true;
    g_test_state.config.type = TEST_TYPE_ALL;
    
    // 设置信号处理
    signal(SIGALRM, test_timeout_handler);
}

// 清理测试框架
void test_framework_cleanup(void) {
    if (!g_test_state.initialized) {
        return;
    }
    
    // 恢复信号处理
    signal(SIGALRM, SIG_DFL);
    g_test_state.initialized = false;
}

// 运行单个测试用例
int run_test_case(const test_case_t* test_case) {
    if (!test_case || !test_case->fn) {
        return -1;
    }

    // 跳过标记为skip的测试
    if (test_case->skip) {
        printf("Skipping test: %s\n", test_case->name);
        return 0;
    }

    // 记录测试名称
    strncpy(current_test_name, test_case->name, sizeof(current_test_name) - 1);
    printf("Running test: %s\n", current_test_name);
    
    // 记录开始时间
    g_test_state.stats.start_time = clock();
    
    // 设置超时处理
    int result = 0;
    if (setjmp(g_test_state.timeout_jmp) == 0) {
        alarm(test_case->timeout_seconds);
        result = test_case->fn();
        alarm(0);
    } else {
        printf("Test timeout: %s\n", current_test_name);
        result = -1;
    }
    
    // 记录结束时间
    g_test_state.stats.end_time = clock();
    
    // 更新测试结果
    if (result == 0) {
        strcpy(current_test_result, "PASS");
    } else {
        strcpy(current_test_result, "FAIL");
        test_case_failed++;
    }
    test_case_count++;
    
    // 打印测试结果
    printf("%s: %s (%.2f seconds)\n", 
           current_test_name,
           current_test_result,
           (double)(g_test_state.stats.end_time - g_test_state.stats.start_time) / CLOCKS_PER_SEC);
    
    return result;
}

// 运行测试套件
int run_test_suite(const test_suite_t* suite) {
    if (!suite) {
        return -1;
    }
    
    printf("\nRunning test suite: %s\n", suite->name);
    
    // 执行套件初始化
    if (suite->setup) {
        suite->setup();
    }
    
    // 运行所有测试用例
    int failed = 0;
    for (size_t i = 0; i < suite->num_cases; i++) {
        if (run_test_case(&suite->cases[i]) != 0) {
            failed++;
        }
    }
    
    // 执行套件清理
    if (suite->teardown) {
        suite->teardown();
    }
    
    return failed;
}

// 检查是否应该运行指定类型的测试
bool test_framework_should_run(test_type_t type) {
    return (g_test_state.config.type & type) != 0;
}

// 打印测试统计信息
void test_print_stats(void) {
    printf("\nTest Summary:\n");
    printf("Total tests: %d\n", test_case_count);
    printf("Failed tests: %d\n", test_case_failed);
    printf("Passed tests: %d\n", test_case_count - test_case_failed);
    printf("Total time: %.2f seconds\n",
           (double)(g_test_state.stats.end_time - g_test_state.stats.start_time) / CLOCKS_PER_SEC);
}

// 获取测试结果
int test_get_result(void) {
    return test_case_failed;
}