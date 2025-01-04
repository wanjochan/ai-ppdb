#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <cosmopolitan.h>

// 测试类型
typedef enum {
    TEST_TYPE_UNIT = 1,    // 单元测试
    TEST_TYPE_PERF = 2,    // 性能测试
    TEST_TYPE_STRESS = 4,  // 压力测试
    TEST_TYPE_ALL = 7      // 所有类型
} test_type_t;

// 测试配置
typedef struct {
    test_type_t type;      // 测试类型
} test_config_t;

// 测试统计
typedef struct {
    clock_t start_time;    // 开始时间
    clock_t end_time;      // 结束时间
    size_t peak_memory;    // 峰值内存
} test_stats_t;

// 测试状态
typedef struct {
    bool initialized;      // 是否已初始化
    test_config_t config;  // 测试配置
    test_stats_t stats;    // 测试统计
    jmp_buf timeout_jmp;   // 超时跳转点
} test_state_t;

// 测试用例
typedef struct {
    const char* name;           // 测试名称
    const char* description;    // 测试描述
    int (*fn)(void);           // 测试函数
    int timeout_seconds;        // 超时时间
    bool skip;                  // 是否跳过
} test_case_t;

// 测试套件
typedef struct {
    const char* name;           // 套件名称
    void (*setup)(void);       // 套件初始化
    void (*teardown)(void);    // 套件清理
    const test_case_t* cases;  // 测试用例数组
    size_t num_cases;          // 测试用例数量
} test_suite_t;

// 全局变量声明
extern test_state_t g_test_state;
extern char current_test_name[256];
extern char current_test_result[32];
extern int test_case_count;
extern int test_case_failed;


// Assert macro for testing
#define ASSERT(condition, format, ...) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "Assertion failed: " format "\n", ##__VA_ARGS__); \
            fprintf(stderr, "  at %s:%d\n", __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

// Test framework functions
void test_framework_init(void);
void test_framework_cleanup(void);
int run_test_case(const test_case_t* test_case);
int run_test_suite(const test_suite_t* suite);
bool test_framework_should_run(test_type_t type);
void test_print_stats(void);

#endif // TEST_FRAMEWORK_H