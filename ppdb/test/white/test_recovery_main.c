#include "test_framework.h"

// 声明测试注册函数
extern void register_recovery_tests(void);

int main(void) {
    test_framework_init();
    register_recovery_tests();
    return test_framework_run();
}