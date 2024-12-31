#ifndef PPDB_TEST_FRAMEWORK_H
#define PPDB_TEST_FRAMEWORK_H

#include "cosmopolitan.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_logger.h"

// 测试类型
typedef enum test_type {
    TEST_TYPE_UNIT,      // 单元测试
    TEST_TYPE_PERF,      // 性能测试
    TEST_TYPE_STRESS,    // 压力测试
    TEST_TYPE_FUZZ,      // 模糊测试
    TEST_TYPE_ALL        // 所有测试
} test_type_t;

// 测试用例结构
typedef struct test_case {
    const char* name;           // 测试名称
    int (*fn)(void);           // 测试函数
    int timeout_seconds;        // 超时时间（秒）
    bool skip;                  // 是否跳过
    const char* description;    // 测试描述
} test_case_t;

// 测试套件结构
typedef struct test_suite {
    const char* name;                  // 套件名称
    const test_case_t* cases;          // 测试用例数组
    size_t num_cases;                  // 测试用例数量
    void (*setup)(void);              // 套件初始化函数
    void (*teardown)(void);           // 套件清理函数
} test_suite_t;

// 测试统计信息
typedef struct test_stats {
    clock_t start_time;        // 开始时间
    clock_t end_time;          // 结束时间
    size_t total_tests;        // 总测试数
    size_t failed_tests;       // 失败测试数
    size_t skipped_tests;      // 跳过测试数
    size_t peak_memory;        // 峰值内存
} test_stats_t;

// 测试配置
typedef struct test_config {
    test_type_t type;          // 测试类型
    bool verbose;              // 是否详细输出
    bool abort_on_failure;     // 失败时是否中止
    bool color_output;         // 是否彩色输出
    const char* filter;        // 测试过滤器
} test_config_t;

// 测试状态
typedef struct test_state {
    test_stats_t stats;        // 统计信息
    test_config_t config;      // 配置信息
    jmp_buf timeout_jmp;       // 超时跳转缓冲区
    bool initialized;          // 是否已初始化
} test_state_t;

// 全局变量
extern test_state_t g_test_state;
extern char current_test_name[256];
extern char current_test_result[32];
extern int test_case_count;
extern int test_case_failed;

// 测试框架初始化和清理
void test_framework_init(void);
void test_framework_cleanup(void);

// 测试宏定义
#define TEST_INIT(name) do { \
    test_framework_init(); \
    printf("Running test suite: %s\n", name); \
} while(0)

#define TEST_SUMMARY() test_print_stats()
#define TEST_RESULT() test_get_result()

#define ASSERT_EQ(actual, expected) do { \
    if ((actual) != (expected)) { \
        printf("Assertion failed: %s == %s\n", #actual, #expected); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_GT(actual, expected) do { \
    if ((actual) <= (expected)) { \
        printf("Assertion failed: %s > %s\n", #actual, #expected); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("Assertion failed: %s is NULL\n", #ptr); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define RUN_TEST(test_fn) do { \
    printf("Running test: %s\n", #test_fn); \
    if (test_fn() != 0) { \
        printf("Test failed: %s\n", #test_fn); \
        return -1; \
    } \
} while(0)

// 检查是否应该运行某个测试
bool test_framework_should_run(test_type_t type);

// 测试套件管理
int run_test_suite(const test_suite_t* suite);
int run_test_case(const test_case_t* test_case);
void test_print_stats(void);

// 测试结果函数
int test_get_result(void);

#endif // PPDB_TEST_FRAMEWORK_H