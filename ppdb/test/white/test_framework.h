#ifndef PPDB_TEST_FRAMEWORK_H
#define PPDB_TEST_FRAMEWORK_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "src/kvstore/internal/kvstore_logger.h"

// 测试统计
typedef struct {
    clock_t start_time;      // 测试开始时间
    clock_t end_time;        // 测试结束时间
    int total_cases;         // 总用例数
    int passed_cases;        // 通过用例数
    int failed_cases;        // 失败用例数
    size_t peak_memory;      // 峰值内存使用
    size_t total_allocated;  // 总分配内存
    size_t current_allocated;// 当前分配内存
} test_stats_t;

// 测试环境配置
typedef struct {
    int thread_count;        // 并发线程数
    int timeout_seconds;     // 超时时间
    size_t memory_limit;     // 内存限制
    char* temp_dir;         // 临时目录
    bool verbose;           // 详细日志
    bool abort_on_failure;  // 失败时终止
} test_config_t;

// 错误注入配置
typedef struct {
    float crash_probability;  // 崩溃概率
    float delay_probability; // 延迟概率
    int max_delay_ms;       // 最大延迟
    bool enabled;           // 是否启用
} error_injection_t;

// 资源跟踪
typedef struct {
    void* ptr;              // 资源指针
    const char* type;       // 资源类型
    const char* file;       // 分配文件
    int line;              // 分配行号
    void (*cleanup)(void*); // 清理函数
} resource_tracker_t;

// 测试用例函数类型
typedef int (*test_case_fn_t)(void);

// 测试用例结构
typedef struct {
    const char* name;           // 用例名称
    test_case_fn_t fn;         // 用例函数
    int timeout_seconds;        // 用例超时时间
    bool skip;                 // 是否跳过
    const char* description;   // 用例描述
} test_case_t;

// 测试套件结构
typedef struct {
    const char* name;          // 套件名称
    const test_case_t* cases;  // 用例数组
    size_t num_cases;          // 用例数量
    void (*setup)(void);      // 套件初始化
    void (*teardown)(void);   // 套件清理
} test_suite_t;

// 框架初始化和清理
void test_framework_init(void);
void test_framework_cleanup(void);

// 配置管理
void test_set_config(const test_config_t* config);
void test_get_config(test_config_t* config);

// 错误注入
void test_set_error_injection(const error_injection_t* config);
void test_inject_error(void);

// 资源管理
void* test_track_resource(void* ptr, const char* type, 
    const char* file, int line, void (*cleanup)(void*));
void test_cleanup_resources(void);

// 统计管理
void test_start_stats(void);
void test_end_stats(void);
void test_get_stats(test_stats_t* stats);
void test_print_stats(void);

// 运行测试
int run_test_suite(const test_suite_t* suite);
int run_single_test(const test_case_t* test);

// 清理测试目录
void cleanup_test_dir(const char* dir_path);

// 测试断言宏
#define TEST_ASSERT(condition, ...) \
    do { \
        if (!(condition)) { \
            ppdb_log_error(__VA_ARGS__); \
            test_cleanup_resources(); \
            return 1; \
        } \
    } while (0)

#define TEST_ASSERT_OK(err, message) \
    TEST_ASSERT((err) == PPDB_OK, "Operation failed: %s (error: %s)", \
        message, ppdb_error_string(err))

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != NULL, "Null pointer: %s", message)

// 资源跟踪宏
#define TEST_TRACK(ptr, type, cleanup_fn) \
    test_track_resource(ptr, type, __FILE__, __LINE__, cleanup_fn)

// 测试注册宏
#define TEST_REGISTER(fn) test_register_case(#fn, fn, 0, false, "")
#define TEST_REGISTER_FULL(fn, timeout, skip, desc) \
    test_register_case(#fn, fn, timeout, skip, desc)

#endif // PPDB_TEST_FRAMEWORK_H