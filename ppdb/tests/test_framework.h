#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <cosmopolitan.h>
#include <ppdb/error.h>
#include <ppdb/logger.h>

// 测试用例结构
typedef struct {
    const char* name;
    int (*func)(void);
} test_case_t;

// 测试套件结构
typedef struct {
    const char* name;
    const test_case_t* cases;
    size_t num_cases;
} test_suite_t;

// 声明测试套件的宏
#define DECLARE_TEST_SUITE(name) \
    extern test_suite_t name##_suite

// 定义测试套件的宏
#define DEFINE_TEST_SUITE(name, test_cases) \
    test_suite_t name##_suite = { \
        .name = #name, \
        .cases = test_cases, \
        .num_cases = sizeof(test_cases) / sizeof(test_cases[0]) \
    }

// 运行测试套件的函数声明
int run_test_suite(const test_suite_t* suite);

// 测试断言宏
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            ppdb_log_error("Assertion failed: %s", message); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_OK(err, message) \
    do { \
        if ((err) != PPDB_OK) { \
            ppdb_log_error("Operation failed: %s (error: %d)", message, err); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            ppdb_log_error("Null pointer: %s", message); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(str1, str2, message) \
    do { \
        if (strcmp((str1), (str2)) != 0) { \
            ppdb_log_error("String mismatch: %s (expected '%s', got '%s')", \
                          message, str2, str1); \
            return 1; \
        } \
    } while (0)

#endif // TEST_FRAMEWORK_H