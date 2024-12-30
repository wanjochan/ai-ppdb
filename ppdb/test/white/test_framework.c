#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_logger.h"
#include "kvstore/internal/kvstore_fs.h"

// 工具函数实现
void microsleep(int microseconds) {
    struct timespec ts = {
        .tv_sec = microseconds / 1000000,
        .tv_nsec = (microseconds % 1000000) * 1000
    };
    nanosleep(&ts, NULL);
}

// 全局测试状态变量定义
char current_test_name[256] = {0};
char current_test_result[32] = {0};
char current_test_message[1024] = {0};
int test_case_count = 0;
int test_case_failed = 0;

#define MAX_RESOURCES 1024
#define DEFAULT_TIMEOUT 30
#define DEFAULT_THREADS 4
#define DEFAULT_MEMORY_LIMIT (1024 * 1024 * 1024)  // 1GB

// 全局状态
static struct {
    test_config_t config;
    error_injection_t error_injection;
    test_stats_t stats;
    resource_tracker_t resources[MAX_RESOURCES];
    size_t resource_count;
    bool initialized;
    jmp_buf timeout_jmp;
    test_type_t test_type;
} g_test_state = {0};

// 段错误处理函数
static void segv_handler(int signo) {
    (void)signo;
    ppdb_log_error("Segmentation fault in test: %s", current_test_name);
    test_cleanup_resources();
    exit(1);
}

// 超时处理函数
static void timeout_handler(int signo) {
    (void)signo;
    ppdb_log_error("Test timeout: %s", current_test_name);
    test_cleanup_resources();
    longjmp(g_test_state.timeout_jmp, 1);
}

// 初始化测试框架
void test_framework_init(void) {
    if (g_test_state.initialized) return;
    
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
    
    // 设置默认配置
    g_test_state.config.thread_count = DEFAULT_THREADS;
    g_test_state.config.timeout_seconds = DEFAULT_TIMEOUT;
    g_test_state.config.memory_limit = DEFAULT_MEMORY_LIMIT;
    g_test_state.config.temp_dir = "./tmp_test";
    g_test_state.config.verbose = true;
    g_test_state.config.abort_on_failure = false;
    
    // 初始化错误注入
    g_test_state.error_injection.enabled = false;
    g_test_state.error_injection.crash_probability = 0.0f;
    g_test_state.error_injection.delay_probability = 0.0f;
    g_test_state.error_injection.max_delay_ms = 100;
    
    // 初始化统计
    memset(&g_test_state.stats, 0, sizeof(test_stats_t));
    g_test_state.stats.start_time = clock();
    
    // 初始化资源跟踪
    g_test_state.resource_count = 0;
    
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
    
    // 创建临时目录
    cleanup_test_dir(g_test_state.config.temp_dir);
    
    // 设置默认测试类型
    char* test_type_env = getenv("TEST_TYPE");
    if (test_type_env) {
        if (strcmp(test_type_env, "unit") == 0) {
            g_test_state.test_type = TEST_TYPE_UNIT;
        } else if (strcmp(test_type_env, "integration") == 0) {
            g_test_state.test_type = TEST_TYPE_INTEGRATION;
        } else if (strcmp(test_type_env, "stress") == 0) {
            g_test_state.test_type = TEST_TYPE_STRESS;
        } else {
            g_test_state.test_type = TEST_TYPE_ALL;
        }
    } else {
        g_test_state.test_type = TEST_TYPE_ALL;
    }
    
    g_test_state.initialized = true;
}

// 清理测试框架
void test_framework_cleanup(void) {
    if (!g_test_state.initialized) return;
    
    test_cleanup_resources();
    cleanup_test_dir(g_test_state.config.temp_dir);
    
    g_test_state.stats.end_time = clock();
    test_print_stats();
    
    g_test_state.initialized = false;
}

// 配置管理
void test_set_config(const test_config_t* config) {
    if (!config) return;
    memcpy(&g_test_state.config, config, sizeof(test_config_t));
}

void test_get_config(test_config_t* config) {
    if (!config) return;
    memcpy(config, &g_test_state.config, sizeof(test_config_t));
}

// 错误注入
void test_set_error_injection(const error_injection_t* config) {
    if (!config) return;
    memcpy(&g_test_state.error_injection, config, sizeof(error_injection_t));
}

void test_inject_error(void) {
    if (!g_test_state.error_injection.enabled) return;
    
    float r = (float)random() / RAND_MAX;
    
    if (r < g_test_state.error_injection.crash_probability) {
        abort();
    }
    
    if (r < g_test_state.error_injection.delay_probability) {
        int delay = random() % g_test_state.error_injection.max_delay_ms;
        microsleep(delay * 1000);
    }
}

// 资源管理
void* test_track_resource(void* ptr, const char* type, 
    const char* file, int line, void (*cleanup)(void*)) {
    if (!ptr || g_test_state.resource_count >= MAX_RESOURCES) return ptr;
    
    resource_tracker_t* tracker = &g_test_state.resources[g_test_state.resource_count++];
    tracker->ptr = ptr;
    tracker->type = type;
    tracker->file = file;
    tracker->line = line;
    tracker->cleanup = cleanup;
    
    return ptr;
}

void test_cleanup_resources(void) {
    for (size_t i = 0; i < g_test_state.resource_count; i++) {
        resource_tracker_t* tracker = &g_test_state.resources[i];
        if (tracker->cleanup && tracker->ptr) {
            tracker->cleanup(tracker->ptr);
            tracker->ptr = NULL;
        }
    }
    g_test_state.resource_count = 0;
}

// 统计管理
void test_start_stats(void) {
    g_test_state.stats.start_time = clock();
}

void test_end_stats(void) {
    g_test_state.stats.end_time = clock();
}

void test_get_stats(test_stats_t* stats) {
    if (!stats) return;
    memcpy(stats, &g_test_state.stats, sizeof(test_stats_t));
}

void test_print_stats(void) {
    double duration = (double)(g_test_state.stats.end_time - g_test_state.stats.start_time) 
        / CLOCKS_PER_SEC;
    
    ppdb_log_info("Test Results:");
    ppdb_log_info("  Total Cases: %d", test_case_count);
    ppdb_log_info("  Failed: %d", test_case_failed);
    ppdb_log_info("  Duration: %.2f seconds", duration);
    ppdb_log_info("  Peak Memory: %zu bytes", g_test_state.stats.peak_memory);
}

// 获取测试结果
int test_get_result(void) {
    return test_case_failed;
}

// 运行单个测试
int run_single_test(const test_case_t* test) {
    if (!test || !test->fn) return 1;
    
    if (test->skip) {
        ppdb_log_info("Skipping test: %s", test->name);
        return 0;
    }
    
    ppdb_log_info("Running test: %s", test->name);
    if (test->description && g_test_state.config.verbose) {
        ppdb_log_info("Description: %s", test->description);
    }
    
    // 设置当前测试名称
    strlcpy(current_test_name, test->name, sizeof(current_test_name));
    
    // 设置超时
    int timeout = test->timeout_seconds > 0 ? 
        test->timeout_seconds : g_test_state.config.timeout_seconds;
    alarm(timeout);
    
    // 执行测试
    int result;
    if (setjmp(g_test_state.timeout_jmp) == 0) {
        result = test->fn();
    } else {
        ppdb_log_error("Test timeout: %s", test->name);
        result = 1;
    }
    
    // 取消超时
    alarm(0);
    
    // 更新统计
    g_test_state.stats.total_cases++;
    if (result == 0) {
        g_test_state.stats.passed_cases++;
        strlcpy(current_test_result, "PASS", sizeof(current_test_result));
    } else {
        g_test_state.stats.failed_cases++;
        strlcpy(current_test_result, "FAIL", sizeof(current_test_result));
    }
    
    // 清理资源
    test_cleanup_resources();
    
    return result;
}

// 运行单个测试用例
int run_test_case(const test_case_t* test_case) {
    if (!test_case) return -1;
    
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
    if (!suite) return -1;
    
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

// 清理测试目录
void cleanup_test_dir(const char* dir_path) {
    if (!dir_path) return;
    
    // 先尝试删除目录
    rmdir(dir_path);
    
    // 创建新目录
    mkdir(dir_path, 0755);
}

// 设置测试类型
void test_framework_set_type(test_type_t type) {
    g_test_state.test_type = type;
}

// 检查是否应该运行某个测试
bool test_framework_should_run(test_type_t type) {
    return g_test_state.test_type == TEST_TYPE_ALL || g_test_state.test_type == type;
}