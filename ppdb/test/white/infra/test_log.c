#include "test/test_common.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_log.h"
#include "test/test_framework.h"

// Basic functionality test
static void test_log_basic(void) {
    const char* test_msg = "Test log message";
    TEST_ASSERT(ppdb_log_init() == PPDB_OK);
    TEST_ASSERT(ppdb_log_write(PPDB_LOG_INFO, test_msg) == PPDB_OK);
    TEST_ASSERT(ppdb_log_check_exists(test_msg));
    ppdb_log_cleanup();
}

// Performance test
static void test_log_performance(void) {
    int64_t start, end;
    const int iterations = 10000;
    
    start = infra_get_time_ms();
    for (int i = 0; i < iterations; i++) {
        ppdb_log_write(PPDB_LOG_INFO, "Performance test message");
    }
    end = infra_get_time_ms();
    
    double time_spent = (end - start) / 1000.0;
    TEST_ASSERT(time_spent < 1.0); // Should complete within 1 second
}

// Boundary conditions test
static void test_log_boundary(void) {
    char* large_msg = infra_malloc(PPDB_MAX_LOG_SIZE + 1);
    infra_memset(large_msg, 'A', PPDB_MAX_LOG_SIZE);
    large_msg[PPDB_MAX_LOG_SIZE] = '\0';
    
    TEST_ASSERT(ppdb_log_write(PPDB_LOG_INFO, "") == PPDB_OK);  // Empty message
    TEST_ASSERT(ppdb_log_write(PPDB_LOG_INFO, large_msg) == PPDB_ERROR_INVALID_ARGUMENT);  // Too large
    
    infra_free(large_msg);
}

// Error handling test
static void test_log_error_handling(void) {
    TEST_ASSERT(ppdb_log_write(PPDB_LOG_INFO, NULL) == PPDB_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(ppdb_log_write(999, "Invalid level") == PPDB_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT(ppdb_log_cleanup() == PPDB_OK);
    TEST_ASSERT(ppdb_log_write(PPDB_LOG_INFO, "After cleanup") == PPDB_ERROR_NOT_INITIALIZED);
}

// Concurrent access test
static void* concurrent_log_thread(void* arg) {
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT(ppdb_log_write(PPDB_LOG_INFO, "Concurrent log") == PPDB_OK);
    }
    return NULL;
}

static void test_log_concurrent(void) {
    ppdb_thread_t* threads[5];
    ppdb_error_t err;
    
    for (int i = 0; i < 5; i++) {
        err = ppdb_thread_create(&threads[i], concurrent_log_thread, NULL);
        TEST_ASSERT(err == PPDB_OK, "Thread creation failed");
    }
    
    for (int i = 0; i < 5; i++) {
        err = ppdb_thread_join(threads[i]);
        TEST_ASSERT(err == PPDB_OK, "Thread join failed");
    }
}

int main(void) {
    infra_printf("Running test suite: Log Tests\n");
    
    infra_printf("  Running test: test_log_basic\n");
    test_log_basic();
    
    infra_printf("  Running test: test_log_performance\n");
    test_log_performance();
    
    infra_printf("  Running test: test_log_boundary\n");
    test_log_boundary();
    
    infra_printf("  Running test: test_log_error_handling\n");
    test_log_error_handling();
    
    infra_printf("  Running test: test_log_concurrent\n");
    test_log_concurrent();
    
    infra_printf("Test suite completed successfully\n");
    return 0;
} 