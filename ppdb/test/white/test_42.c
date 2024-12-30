#include <cosmopolitan.h>
#include "test_framework.h"

// 测试返回42
static int test_return_42(void) {
    printf("Testing return 42...\n");
    return 0;  // 0表示测试成功
}

// 测试套件定义
static const test_case_t test_cases[] = {
    {"test_return_42", test_return_42, 0, false, "Test that always succeeds"}
};

static const test_suite_t test_suite = {
    .name = "42 Test Suite",
    .cases = test_cases,
    .num_cases = sizeof(test_cases) / sizeof(test_case_t),
    .setup = NULL,
    .teardown = NULL
};

// Windows入口点
int WinMain(void) {
    // 初始化测试框架
    test_framework_init();
    
    // 运行测试套件
    int failed = run_test_suite(&test_suite);
    
    // 打印测试统计
    test_print_stats();
    
    // 清理测试框架
    test_framework_cleanup();
    
    return failed ? 1 : 0;
}

// Unix入口点
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return WinMain();
} 