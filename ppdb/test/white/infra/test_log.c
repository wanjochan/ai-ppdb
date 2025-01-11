#include "test_common.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "test_framework.h"

static bool log_message_found = false;
static char last_log_message[4096];

static void test_log_callback(int level, const char* file, int line,
                            const char* func, const char* msg) {
    (void)level;  // 未使用的参数
    (void)file;   // 未使用的参数
    (void)line;   // 未使用的参数
    (void)func;   // 未使用的参数
    
    infra_strcpy(last_log_message, msg);
    log_message_found = true;
}

// Basic functionality test
static int test_log_basic(void) {
    const char* test_msg = "Test log message";
    log_message_found = false;
    
    infra_log_set_callback(test_log_callback);
    infra_log_set_level(INFRA_LOG_LEVEL_INFO);
    
    INFRA_LOG_INFO("%s", test_msg);
    TEST_ASSERT(log_message_found);
    TEST_ASSERT(infra_strcmp(last_log_message, test_msg) == 0);
    return 0;
}

// Performance test
static int test_log_performance(void) {
    infra_time_t start, end;
    const int iterations = 100;
    
    start = infra_time_monotonic();
    for (int i = 0; i < iterations; i++) {
        INFRA_LOG_INFO("Performance test message");
    }
    end = infra_time_monotonic();
    
    double time_spent = (double)(end - start) / 1000000.0;  // Convert to seconds
    TEST_ASSERT(time_spent < 30.0);
    return 0;
}

// Boundary conditions test
static int test_log_boundary(void) {
    char* large_msg = infra_malloc(4096);
    infra_memset(large_msg, 'A', 4095);
    large_msg[4095] = '\0';
    
    INFRA_LOG_INFO("");  // Empty message
    INFRA_LOG_INFO("%s", large_msg);  // Large message
    
    infra_free(large_msg);
    return 0;
}

// Error handling test
static int test_log_error_handling(void) {
    log_message_found = false;
    infra_log_set_level(INFRA_LOG_LEVEL_NONE);
    INFRA_LOG_INFO("Should not appear");
    TEST_ASSERT(!log_message_found);
    
    log_message_found = false;
    infra_log_set_level(999);  // Invalid level
    INFRA_LOG_INFO("Should not appear");
    TEST_ASSERT(!log_message_found);
    
    log_message_found = false;
    infra_log_set_callback(NULL);  // Remove callback
    INFRA_LOG_INFO("Should not trigger callback");
    TEST_ASSERT(!log_message_found);
    return 0;
}

// Concurrent access test
static void* concurrent_log_thread(void* arg) {
    (void)arg;  // 未使用的参数
    for (int i = 0; i < 1000; i++) {
        INFRA_LOG_INFO("Concurrent log");
    }
    return NULL;
}

static int test_log_concurrent(void) {
    infra_thread_t threads[5];
    infra_error_t err;
    
    infra_log_set_callback(test_log_callback);
    infra_log_set_level(INFRA_LOG_LEVEL_INFO);
    
    for (int i = 0; i < 5; i++) {
        err = infra_thread_create(&threads[i], concurrent_log_thread, NULL);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    for (int i = 0; i < 5; i++) {
        err = infra_thread_join(threads[i]);
        TEST_ASSERT(err == INFRA_OK);
    }
    return 0;
}

int main(void) {
    // 初始化infra系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    TEST_INIT();
    
    infra_printf("Running test suite: Log Tests\n");
    
    TEST_RUN(test_log_basic);
    TEST_RUN(test_log_performance);
    TEST_RUN(test_log_boundary);
    TEST_RUN(test_log_error_handling);
    TEST_RUN(test_log_concurrent);
    
    TEST_CLEANUP();
    
    // 清理infra系统
    infra_cleanup();
    return 0;
} 