#include "internal/infra/infra.h"
#include "test_framework.h"

// 测试基础设施层初始化
static int test_infra_init(void) {
    infra_config_t config;
    TEST_ASSERT(infra_config_init(&config) == INFRA_OK, "Failed to init config");
    TEST_ASSERT(infra_init(&config) == INFRA_OK, "Failed to initialize infra");
    TEST_ASSERT(infra_cleanup() == INFRA_OK, "Failed to cleanup infra");
    return 0;
}

// 定义测试用例
static test_case_info_t test_cases[] = {
    {"test_infra_init", test_infra_init}
};

// 主函数
int main(void) {
    return run_test_suite(test_cases, sizeof(test_cases) / sizeof(test_cases[0]));
} 