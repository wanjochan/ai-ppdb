#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include "test_framework.h"

static void test_log_basic() {
    ppdb_error_t err;
    
    // Test initialization
    err = ppdb_log_init("test.log", PPDB_LOG_INFO, true);
    ASSERT_OK(err);
    
    // Test different log levels
    ppdb_log_info("Test info message");
    ppdb_log_warn("Test warning message");
    ppdb_log_error("Test error message");
    
    // Test format strings
    ppdb_log_info("Test number: %d", 42);
    ppdb_log_info("Test string: %s", "hello");
    
    // Test cleanup
    ppdb_log_close();
    
    // Test double initialization
    err = ppdb_log_init("test.log", PPDB_LOG_INFO, true);
    ASSERT_OK(err);
    ppdb_log_close();
}

static void test_log_levels() {
    ppdb_error_t err;
    
    // Test debug level
    err = ppdb_log_init("test_debug.log", PPDB_LOG_DEBUG, false);
    ASSERT_OK(err);
    ppdb_log_debug("This should be logged");
    ppdb_log_close();
    
    // Test info level
    err = ppdb_log_init("test_info.log", PPDB_LOG_INFO, false);
    ASSERT_OK(err);
    ppdb_log_debug("This should not be logged");
    ppdb_log_info("This should be logged");
    ppdb_log_close();
    
    // Test error level
    err = ppdb_log_init("test_error.log", PPDB_LOG_ERROR, false);
    ASSERT_OK(err);
    ppdb_log_warn("This should not be logged");
    ppdb_log_error("This should be logged");
    ppdb_log_close();
}

static void test_log_concurrent() {
    ppdb_error_t err;
    
    // Initialize with console output for visibility
    err = ppdb_log_init("test_concurrent.log", PPDB_LOG_INFO, true);
    ASSERT_OK(err);
    
    // Log from multiple threads
    #define NUM_THREADS 4
    #define MSGS_PER_THREAD 100
    
    pthread_t threads[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, [](void* arg) -> void* {
            int thread_id = *(int*)arg;
            for (int j = 0; j < MSGS_PER_THREAD; j++) {
                ppdb_log_info("Thread %d: Message %d", thread_id, j);
            }
            return NULL;
        }, &i);
    }
    
    // Wait for all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    ppdb_log_close();
}

int main() {
    TEST_SUITE_BEGIN("Log Tests");
    
    TEST_RUN(test_log_basic);
    TEST_RUN(test_log_levels);
    TEST_RUN(test_log_concurrent);
    
    TEST_SUITE_END();
    return 0;
} 