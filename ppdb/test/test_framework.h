#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include "internal/infra/infra.h"

// 测试断言宏
#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        infra_printf("ASSERT FAILED: %s\n", msg); \
        return -1; \
    }

// 测试用例函数类型
typedef int (*test_case_t)(void);

// 测试用例结构
typedef struct {
    const char* name;
    test_case_t func;
} test_case_info_t;

// 运行测试用例
static inline int run_test_case(const test_case_info_t* test) {
    infra_printf("Running test: %s\n", test->name);
    int result = test->func();
    if (result == 0) {
        infra_printf("Test passed: %s\n", test->name);
    } else {
        infra_printf("Test failed: %s (error code: %d)\n", test->name, result);
    }
    return result;
}

// 运行测试套件
static inline int run_test_suite(const test_case_info_t* tests, int count) {
    int failed = 0;
    infra_printf("Running %d tests...\n", count);
    
    for (int i = 0; i < count; i++) {
        if (run_test_case(&tests[i]) != 0) {
            failed++;
        }
    }
    
    infra_printf("\nTest summary:\n");
    infra_printf("Total tests: %d\n", count);
    infra_printf("Passed: %d\n", count - failed);
    infra_printf("Failed: %d\n", failed);
    
    return failed;
}

#endif // TEST_FRAMEWORK_H 