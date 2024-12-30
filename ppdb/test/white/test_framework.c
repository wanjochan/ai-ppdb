#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_logger.h"
#include "kvstore/internal/kvstore_fs.h"

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
} g_test_state = {0};

// 超时信号处理
static void timeout_handler(int signo) {
    (void)signo;
    siglongjmp(g_test_state.timeout_jmp, 1);
}

// 初始化测试框架
void test_framework_init(void) {
    if (g_test_state.initialized) return;
    
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
    
    // 初始化资源跟踪
    g_test_state.resource_count = 0;
    
    // 设置超时处理
    sigaction(SIGALRM, &(struct sigaction){.sa_handler = timeout_handler}, NULL);
    
    g_test_state.initialized = true;
}

// 清理测试框架
void test_framework_cleanup(void) {
    if (!g_test_state.initialized) return;
    
    test_cleanup_resources();
    cleanup_test_dir(g_test_state.config.temp_dir);
    
    g_test_state.initialized = false;
}

// 配置管理
void test_set_config(const test_config_t* config) {
    memcpy(&g_test_state.config, config, sizeof(test_config_t));
}

void test_get_config(test_config_t* config) {
    memcpy(config, &g_test_state.config, sizeof(test_config_t));
}

// 错误注入
void test_set_error_injection(const error_injection_t* config) {
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
    memcpy(stats, &g_test_state.stats, sizeof(test_stats_t));
}

void test_print_stats(void) {
    double duration = (double)(g_test_state.stats.end_time - g_test_state.stats.start_time) 
        / CLOCKS_PER_SEC;
    
    ppdb_log_info("Test Results:");
    ppdb_log_info("  Total Cases: %d", g_test_state.stats.total_cases);
    ppdb_log_info("  Passed: %d", g_test_state.stats.passed_cases);
    ppdb_log_info("  Failed: %d", g_test_state.stats.failed_cases);
    ppdb_log_info("  Duration: %.2f seconds", duration);
    ppdb_log_info("  Peak Memory: %zu bytes", g_test_state.stats.peak_memory);
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
    } else {
        g_test_state.stats.failed_cases++;
    }
    
    // 清理资源
    test_cleanup_resources();
    
    return result;
}

// 运行测试套件
int run_test_suite(const test_suite_t* suite) {
    if (!suite) return 1;
    
    ppdb_log_info("Running test suite: %s", suite->name);
    
    // 执行套件初始化
    if (suite->setup) {
        suite->setup();
    }
    
    // 运行所有测试
    int failed = 0;
    for (size_t i = 0; i < suite->num_cases; i++) {
        if (run_single_test(&suite->cases[i]) != 0) {
            failed++;
            if (g_test_state.config.abort_on_failure) {
                break;
            }
        }
    }
    
    // 执行套件清理
    if (suite->teardown) {
        suite->teardown();
    }
    
    return failed;
}

// 清理测试目录
void cleanup_test_dir(const char* dir_path) {
    if (!dir_path) return;
    
    char cmd[256];
    strlcpy(cmd, "rm -rf ", sizeof(cmd));
    strlcat(cmd, dir_path, sizeof(cmd));
    system(cmd);
    
    mkdir(dir_path, 0755);
}

void test_case_start(const char* test_name) {
    strlcpy(current_test_name, test_name, sizeof(current_test_name));
    strlcpy(current_test_result, "PASS", sizeof(current_test_result));
    test_case_count++;
}

void test_case_fail(const char* fmt, ...) {
    strlcpy(current_test_result, "FAIL", sizeof(current_test_result));
    va_list args;
    va_start(args, fmt);
    vsnprintf(current_test_message, sizeof(current_test_message), fmt, args);
    va_end(args);
    test_case_failed++;
}