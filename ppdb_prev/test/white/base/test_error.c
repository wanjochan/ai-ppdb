#include <cosmopolitan.h>
#include "internal/base.h"
#include "test_framework.h"

// Test data
static int callback_count = 0;
static ppdb_error_t last_error_code = PPDB_OK;
static ppdb_error_severity_t last_error_severity = PPDB_ERROR_SEVERITY_INFO;
static ppdb_error_category_t last_error_category = PPDB_ERROR_CATEGORY_SYSTEM;

// Error callback
static void test_error_callback(ppdb_error_t code,
                             ppdb_error_severity_t severity,
                             ppdb_error_category_t category,
                             const char* message,
                             void* user_data) {
    callback_count++;
    last_error_code = code;
    last_error_severity = severity;
    last_error_category = category;
}

// Test basic error handling
static int test_error_basic(void) {
    // Initialize
    ASSERT_OK(ppdb_base_error_init());
    
    // Set error
    ppdb_base_error_set(PPDB_ERR_MEMORY,
                      PPDB_ERROR_SEVERITY_ERROR,
                      PPDB_ERROR_CATEGORY_MEMORY,
                      __FILE__, __LINE__, __func__,
                      "Memory allocation failed: %s", "test error");
    
    // Check context
    const ppdb_error_context_t* ctx = ppdb_base_error_get_context();
    ASSERT_NOT_NULL(ctx);
    ASSERT_EQ(ctx->code, PPDB_ERR_MEMORY);
    ASSERT_EQ(ctx->severity, PPDB_ERROR_SEVERITY_ERROR);
    ASSERT_EQ(ctx->category, PPDB_ERROR_CATEGORY_MEMORY);
    ASSERT_STR_EQ(ctx->message, "Memory allocation failed: test error");
    
    // Check statistics
    ppdb_error_stats_t stats;
    ASSERT_OK(ppdb_base_error_get_stats(&stats));
    ASSERT_EQ(stats.total_errors, 1);
    ASSERT_EQ(stats.errors_by_severity[PPDB_ERROR_SEVERITY_ERROR], 1);
    ASSERT_EQ(stats.errors_by_category[PPDB_ERROR_CATEGORY_MEMORY], 1);
    
    // Clear context
    ppdb_base_error_clear_context();
    ctx = ppdb_base_error_get_context();
    ASSERT_EQ(ctx->code, PPDB_OK);
    
    // Cleanup
    ppdb_base_error_cleanup();
    return 0;
}

// Test error stack
static int test_error_stack(void) {
    // Initialize
    ASSERT_OK(ppdb_base_error_init());
    
    // Push frames
    ASSERT_OK(ppdb_base_error_push_frame(__FILE__, __LINE__, __func__,
                                      "Frame 1: %s", "test error"));
    ASSERT_OK(ppdb_base_error_push_frame(__FILE__, __LINE__, __func__,
                                      "Frame 2: %s", "another error"));
    
    // Check frames
    const ppdb_error_context_t* ctx = ppdb_base_error_get_context();
    ASSERT_NOT_NULL(ctx->stack);
    ASSERT_STR_EQ(ctx->stack->message, "Frame 2: another error");
    ASSERT_STR_EQ(ctx->stack->next->message, "Frame 1: test error");
    
    // Pop frame
    ppdb_base_error_pop_frame();
    ctx = ppdb_base_error_get_context();
    ASSERT_STR_EQ(ctx->stack->message, "Frame 1: test error");
    
    // Cleanup
    ppdb_base_error_cleanup();
    return 0;
}

// Test error callback
static int test_error_callback_func(void) {
    // Initialize
    ASSERT_OK(ppdb_base_error_init());
    
    // Reset test data
    callback_count = 0;
    last_error_code = PPDB_OK;
    last_error_severity = PPDB_ERROR_SEVERITY_INFO;
    last_error_category = PPDB_ERROR_CATEGORY_SYSTEM;
    
    // Set callback
    ASSERT_OK(ppdb_base_error_set_callback(test_error_callback, NULL));
    
    // Set error
    ppdb_base_error_set(PPDB_ERR_IO,
                      PPDB_ERROR_SEVERITY_ERROR,
                      PPDB_ERROR_CATEGORY_IO,
                      __FILE__, __LINE__, __func__,
                      "IO error: %s", "test error");
    
    // Check callback
    ASSERT_EQ(callback_count, 1);
    ASSERT_EQ(last_error_code, PPDB_ERR_IO);
    ASSERT_EQ(last_error_severity, PPDB_ERROR_SEVERITY_ERROR);
    ASSERT_EQ(last_error_category, PPDB_ERROR_CATEGORY_IO);
    
    // Cleanup
    ppdb_base_error_cleanup();
    return 0;
}

// Test error statistics
static int test_error_stats(void) {
    // Initialize
    ASSERT_OK(ppdb_base_error_init());
    
    // Set multiple errors
    ppdb_base_error_set(PPDB_ERR_MEMORY,
                      PPDB_ERROR_SEVERITY_ERROR,
                      PPDB_ERROR_CATEGORY_MEMORY,
                      __FILE__, __LINE__, __func__,
                      "Memory error");
    
    ppdb_base_error_set(PPDB_ERR_IO,
                      PPDB_ERROR_SEVERITY_WARNING,
                      PPDB_ERROR_CATEGORY_IO,
                      __FILE__, __LINE__, __func__,
                      "IO warning");
    
    ppdb_base_error_set(PPDB_ERR_NETWORK,
                      PPDB_ERROR_SEVERITY_FATAL,
                      PPDB_ERROR_CATEGORY_NETWORK,
                      __FILE__, __LINE__, __func__,
                      "Network error");
    
    // Check statistics
    ppdb_error_stats_t stats;
    ASSERT_OK(ppdb_base_error_get_stats(&stats));
    ASSERT_EQ(stats.total_errors, 3);
    ASSERT_EQ(stats.errors_by_severity[PPDB_ERROR_SEVERITY_WARNING], 1);
    ASSERT_EQ(stats.errors_by_severity[PPDB_ERROR_SEVERITY_ERROR], 1);
    ASSERT_EQ(stats.errors_by_severity[PPDB_ERROR_SEVERITY_FATAL], 1);
    ASSERT_EQ(stats.errors_by_category[PPDB_ERROR_CATEGORY_MEMORY], 1);
    ASSERT_EQ(stats.errors_by_category[PPDB_ERROR_CATEGORY_IO], 1);
    ASSERT_EQ(stats.errors_by_category[PPDB_ERROR_CATEGORY_NETWORK], 1);
    
    // Clear statistics
    ppdb_base_error_clear_stats();
    ASSERT_OK(ppdb_base_error_get_stats(&stats));
    ASSERT_EQ(stats.total_errors, 0);
    
    // Cleanup
    ppdb_base_error_cleanup();
    return 0;
}

// Test concurrent error handling
static void error_thread_func(void* arg) {
    int thread_id = *(int*)arg;
    
    for (int i = 0; i < 100; i++) {
        ppdb_base_error_set(PPDB_ERR_MEMORY + (i % 3),
                         PPDB_ERROR_SEVERITY_ERROR,
                         PPDB_ERROR_CATEGORY_MEMORY,
                         __FILE__, __LINE__, __func__,
                         "Thread %d error %d", thread_id, i);
        
        ppdb_base_error_push_frame(__FILE__, __LINE__, __func__,
                                "Thread %d frame %d", thread_id, i);
        
        ppdb_base_sleep(1);
        
        ppdb_base_error_pop_frame();
    }
}

static int test_error_concurrent(void) {
    // Initialize
    ASSERT_OK(ppdb_base_error_init());
    
    // Create threads
    ppdb_base_thread_t* threads[4];
    int thread_ids[4];
    
    for (int i = 0; i < 4; i++) {
        thread_ids[i] = i;
        ASSERT_OK(ppdb_base_thread_create(&threads[i], error_thread_func, &thread_ids[i]));
    }
    
    // Wait for threads
    for (int i = 0; i < 4; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i], NULL));
        ASSERT_OK(ppdb_base_thread_destroy(threads[i]));
    }
    
    // Check statistics
    ppdb_error_stats_t stats;
    ASSERT_OK(ppdb_base_error_get_stats(&stats));
    ASSERT_EQ(stats.total_errors, 400);
    
    // Cleanup
    ppdb_base_error_cleanup();
    return 0;
}

static void test_error_stats_basic(void) {
    ppdb_error_stats_t stats;
    
    // Reset stats
    TEST_ASSERT(ppdb_base_error_reset_stats() == PPDB_OK);
    
    // Record some errors
    TEST_ASSERT(ppdb_base_error_record(PPDB_ERR_MEMORY, 
        PPDB_ERROR_SEVERITY_ERROR, PPDB_ERROR_CATEGORY_MEMORY) == PPDB_OK);
    TEST_ASSERT(ppdb_base_error_record(PPDB_ERR_IO, 
        PPDB_ERROR_SEVERITY_WARNING, PPDB_ERROR_CATEGORY_IO) == PPDB_OK);
    
    // Get stats
    TEST_ASSERT(ppdb_base_error_get_stats(&stats) == PPDB_OK);
    
    // Verify counts
    TEST_ASSERT(stats.total_errors == 2);
    TEST_ASSERT(stats.errors_by_severity[PPDB_ERROR_SEVERITY_WARNING] == 1);
    TEST_ASSERT(stats.errors_by_severity[PPDB_ERROR_SEVERITY_ERROR] == 1);
    TEST_ASSERT(stats.errors_by_category[PPDB_ERROR_CATEGORY_MEMORY] == 1);
    TEST_ASSERT(stats.errors_by_category[PPDB_ERROR_CATEGORY_IO] == 1);
}

static void test_error_trend_analysis(void) {
    ppdb_error_trend_t trend;
    
    // Reset stats
    TEST_ASSERT(ppdb_base_error_reset_stats() == PPDB_OK);
    
    // Record errors with different severities and categories
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(ppdb_base_error_record(PPDB_ERR_MEMORY, 
            PPDB_ERROR_SEVERITY_ERROR, PPDB_ERROR_CATEGORY_MEMORY) == PPDB_OK);
        ppdb_base_sleep_us(100000); // Sleep 100ms
    }
    
    // Analyze trend for last second
    TEST_ASSERT(ppdb_base_error_analyze_trend(1, &trend) == PPDB_OK);
    
    // Verify trend data
    TEST_ASSERT(trend.error_count == 10);
    TEST_ASSERT(trend.highest_severity == PPDB_ERROR_SEVERITY_ERROR);
    TEST_ASSERT(trend.main_category == PPDB_ERROR_CATEGORY_MEMORY);
    TEST_ASSERT(trend.avg_error_rate >= 9.0 && trend.avg_error_rate <= 11.0);
}

static void test_error_stats_thread_safety(void) {
    ppdb_error_stats_t stats;
    ppdb_base_thread_t threads[4];
    int thread_count = 4;
    
    // Reset stats
    TEST_ASSERT(ppdb_base_error_reset_stats() == PPDB_OK);
    
    // Create threads to record errors concurrently
    for (int i = 0; i < thread_count; i++) {
        TEST_ASSERT(ppdb_base_thread_create(&threads[i], NULL, NULL) == PPDB_OK);
    }
    
    // Wait for threads to finish
    for (int i = 0; i < thread_count; i++) {
        TEST_ASSERT(ppdb_base_thread_join(threads[i], NULL) == PPDB_OK);
    }
    
    // Get final stats
    TEST_ASSERT(ppdb_base_error_get_stats(&stats) == PPDB_OK);
    
    // Each thread records 100 errors
    TEST_ASSERT(stats.total_errors == thread_count * 100);
}

static void test_error_log_basic(void) {
    ppdb_error_log_config_t config = {
        .log_dir = "test_logs",
        .max_file_size = 4096,
        .max_files = 3,
        .compress_old = false,
        .async_write = false
    };
    
    // Initialize logging
    TEST_ASSERT(ppdb_base_error_log_init(&config) == PPDB_OK);
    
    // Write some errors
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(ppdb_base_error_log_write(PPDB_ERR_MEMORY,
                                           PPDB_ERROR_SEVERITY_ERROR,
                                           PPDB_ERROR_CATEGORY_MEMORY,
                                           __FILE__, __LINE__, __func__,
                                           "Test error message") == PPDB_OK);
    }
    
    // Verify log file exists
    bool exists;
    TEST_ASSERT(ppdb_base_fs_exists("test_logs/error.0.log", &exists) == PPDB_OK);
    TEST_ASSERT(exists);
    
    // Cleanup
    TEST_ASSERT(ppdb_base_error_log_cleanup() == PPDB_OK);
}

static void test_error_log_rotation(void) {
    ppdb_error_log_config_t config = {
        .log_dir = "test_logs",
        .max_file_size = 256,  // Small size to trigger rotation
        .max_files = 3,
        .compress_old = false,
        .async_write = false
    };
    
    // Initialize logging
    TEST_ASSERT(ppdb_base_error_log_init(&config) == PPDB_OK);
    
    // Write enough errors to trigger rotation
    char message[200];
    for (int i = 0; i < 10; i++) {
        snprintf(message, sizeof(message), "Test error message %d with some padding to make it longer", i);
        TEST_ASSERT(ppdb_base_error_log_write(PPDB_ERR_MEMORY,
                                           PPDB_ERROR_SEVERITY_ERROR,
                                           PPDB_ERROR_CATEGORY_MEMORY,
                                           __FILE__, __LINE__, __func__,
                                           message) == PPDB_OK);
    }
    
    // Verify multiple log files exist
    bool exists;
    TEST_ASSERT(ppdb_base_fs_exists("test_logs/error.0.log", &exists) == PPDB_OK);
    TEST_ASSERT(exists);
    TEST_ASSERT(ppdb_base_fs_exists("test_logs/error.1.log", &exists) == PPDB_OK);
    TEST_ASSERT(exists);
    
    // Cleanup
    TEST_ASSERT(ppdb_base_error_log_cleanup() == PPDB_OK);
}

static void test_error_log_async(void) {
    ppdb_error_log_config_t config = {
        .log_dir = "test_logs",
        .max_file_size = 4096,
        .max_files = 3,
        .compress_old = false,
        .async_write = true
    };
    
    // Initialize logging
    TEST_ASSERT(ppdb_base_error_log_init(&config) == PPDB_OK);
    
    // Write errors asynchronously
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT(ppdb_base_error_log_write(PPDB_ERR_MEMORY,
                                           PPDB_ERROR_SEVERITY_ERROR,
                                           PPDB_ERROR_CATEGORY_MEMORY,
                                           __FILE__, __LINE__, __func__,
                                           "Async test error message") == PPDB_OK);
    }
    
    // Sleep to allow async writes to complete
    ppdb_base_sleep_us(100000);  // 100ms
    
    // Verify log file exists and has content
    bool exists;
    TEST_ASSERT(ppdb_base_fs_exists("test_logs/error.0.log", &exists) == PPDB_OK);
    TEST_ASSERT(exists);
    
    // Cleanup
    TEST_ASSERT(ppdb_base_error_log_cleanup() == PPDB_OK);
}

static void test_error_recovery_basic(void) {
    ppdb_error_recovery_config_t config = {
        .policy = PPDB_ERROR_RECOVERY_RETRY,
        .max_retries = 3,
        .retry_interval_ms = 100,
        .exponential_backoff = false,
        .fallback_handler = NULL
    };
    
    // Initialize recovery system
    TEST_ASSERT(ppdb_base_error_recovery_init(&config) == PPDB_OK);
    
    // Begin recovery
    ppdb_error_recovery_context_t* ctx = NULL;
    TEST_ASSERT(ppdb_base_error_recovery_begin(PPDB_ERR_IO, &ctx) == PPDB_OK);
    TEST_ASSERT(ctx != NULL);
    
    // Test retry logic
    bool should_retry;
    TEST_ASSERT(ppdb_base_error_recovery_should_retry(ctx, &should_retry) == PPDB_OK);
    TEST_ASSERT(should_retry);  // First retry should be allowed
    
    // End recovery
    TEST_ASSERT(ppdb_base_error_recovery_end(ctx, true) == PPDB_OK);
    
    // Cleanup
    TEST_ASSERT(ppdb_base_error_recovery_cleanup() == PPDB_OK);
}

static void test_error_recovery_exponential_backoff(void) {
    ppdb_error_recovery_config_t config = {
        .policy = PPDB_ERROR_RECOVERY_RETRY,
        .max_retries = 5,
        .retry_interval_ms = 100,
        .exponential_backoff = true,
        .fallback_handler = NULL
    };
    
    // Initialize recovery system
    TEST_ASSERT(ppdb_base_error_recovery_init(&config) == PPDB_OK);
    
    // Begin recovery
    ppdb_error_recovery_context_t* ctx = NULL;
    TEST_ASSERT(ppdb_base_error_recovery_begin(PPDB_ERR_IO, &ctx) == PPDB_OK);
    
    // Test exponential backoff
    uint32_t interval;
    TEST_ASSERT(ppdb_base_error_recovery_get_next_retry_interval(ctx, &interval) == PPDB_OK);
    TEST_ASSERT(interval == 100);  // First interval
    
    ctx->retry_count = 1;
    TEST_ASSERT(ppdb_base_error_recovery_get_next_retry_interval(ctx, &interval) == PPDB_OK);
    TEST_ASSERT(interval == 200);  // Second interval
    
    ctx->retry_count = 2;
    TEST_ASSERT(ppdb_base_error_recovery_get_next_retry_interval(ctx, &interval) == PPDB_OK);
    TEST_ASSERT(interval == 400);  // Third interval
    
    // End recovery
    TEST_ASSERT(ppdb_base_error_recovery_end(ctx, true) == PPDB_OK);
    
    // Cleanup
    TEST_ASSERT(ppdb_base_error_recovery_cleanup() == PPDB_OK);
}

static void test_error_recovery_max_retries(void) {
    ppdb_error_recovery_config_t config = {
        .policy = PPDB_ERROR_RECOVERY_RETRY,
        .max_retries = 2,
        .retry_interval_ms = 100,
        .exponential_backoff = false,
        .fallback_handler = NULL
    };
    
    // Initialize recovery system
    TEST_ASSERT(ppdb_base_error_recovery_init(&config) == PPDB_OK);
    
    // Begin recovery
    ppdb_error_recovery_context_t* ctx = NULL;
    TEST_ASSERT(ppdb_base_error_recovery_begin(PPDB_ERR_IO, &ctx) == PPDB_OK);
    
    // Test max retries
    bool should_retry;
    
    // First retry
    TEST_ASSERT(ppdb_base_error_recovery_should_retry(ctx, &should_retry) == PPDB_OK);
    TEST_ASSERT(should_retry);
    
    // Second retry
    ppdb_base_sleep_us(100000);  // Wait for retry interval
    TEST_ASSERT(ppdb_base_error_recovery_should_retry(ctx, &should_retry) == PPDB_OK);
    TEST_ASSERT(should_retry);
    
    // Third retry (should be denied)
    ppdb_base_sleep_us(100000);  // Wait for retry interval
    TEST_ASSERT(ppdb_base_error_recovery_should_retry(ctx, &should_retry) == PPDB_OK);
    TEST_ASSERT(!should_retry);
    
    // End recovery
    TEST_ASSERT(ppdb_base_error_recovery_end(ctx, false) == PPDB_OK);
    
    // Cleanup
    TEST_ASSERT(ppdb_base_error_recovery_cleanup() == PPDB_OK);
}

// Test fallback handler
static ppdb_error_t test_fallback_handler(ppdb_error_t error,
                                       void* context,
                                       void* user_data) {
    int* counter = (int*)user_data;
    (*counter)++;
    return PPDB_OK;
}

static void test_error_recovery_fallback(void) {
    int fallback_counter = 0;
    
    ppdb_error_fallback_config_t fallback_config = {
        .handler = test_fallback_handler,
        .context = NULL,
        .user_data = &fallback_counter,
        .auto_fallback = true
    };
    
    ppdb_error_recovery_config_t config = {
        .policy = PPDB_ERROR_RECOVERY_RETRY,
        .max_retries = 2,
        .retry_interval_ms = 100,
        .exponential_backoff = false,
        .fallback = fallback_config
    };
    
    // Initialize recovery system
    TEST_ASSERT(ppdb_base_error_recovery_init(&config) == PPDB_OK);
    
    // Begin recovery
    ppdb_error_recovery_context_t* ctx = NULL;
    TEST_ASSERT(ppdb_base_error_recovery_begin(PPDB_ERR_IO, &ctx) == PPDB_OK);
    
    // Exhaust retries
    bool should_retry;
    for (int i = 0; i < 3; i++) {
        ppdb_base_sleep_us(100000);  // Wait for retry interval
        TEST_ASSERT(ppdb_base_error_recovery_should_retry(ctx, &should_retry) == PPDB_OK);
    }
    
    // Verify fallback was called
    TEST_ASSERT(fallback_counter == 1);
    
    // End recovery
    TEST_ASSERT(ppdb_base_error_recovery_end(ctx, true) == PPDB_OK);
    
    // Cleanup
    TEST_ASSERT(ppdb_base_error_recovery_cleanup() == PPDB_OK);
}

static void test_error_recovery_direct_fallback(void) {
    int fallback_counter = 0;
    
    ppdb_error_fallback_config_t fallback_config = {
        .handler = test_fallback_handler,
        .context = NULL,
        .user_data = &fallback_counter,
        .auto_fallback = false
    };
    
    ppdb_error_recovery_config_t config = {
        .policy = PPDB_ERROR_RECOVERY_FALLBACK,
        .max_retries = 0,
        .retry_interval_ms = 0,
        .exponential_backoff = false,
        .fallback = fallback_config
    };
    
    // Initialize recovery system
    TEST_ASSERT(ppdb_base_error_recovery_init(&config) == PPDB_OK);
    
    // Begin recovery
    ppdb_error_recovery_context_t* ctx = NULL;
    TEST_ASSERT(ppdb_base_error_recovery_begin(PPDB_ERR_IO, &ctx) == PPDB_OK);
    
    // Try recovery (should go directly to fallback)
    bool should_retry;
    TEST_ASSERT(ppdb_base_error_recovery_should_retry(ctx, &should_retry) == PPDB_OK);
    
    // Verify fallback was called
    TEST_ASSERT(fallback_counter == 1);
    
    // End recovery
    TEST_ASSERT(ppdb_base_error_recovery_end(ctx, true) == PPDB_OK);
    
    // Cleanup
    TEST_ASSERT(ppdb_base_error_recovery_cleanup() == PPDB_OK);
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_error_stats_basic);
    TEST_RUN(test_error_trend_analysis);
    TEST_RUN(test_error_stats_thread_safety);
    TEST_RUN(test_error_log_basic);
    TEST_RUN(test_error_log_rotation);
    TEST_RUN(test_error_log_async);
    TEST_RUN(test_error_recovery_basic);
    TEST_RUN(test_error_recovery_exponential_backoff);
    TEST_RUN(test_error_recovery_max_retries);
    TEST_RUN(test_error_recovery_fallback);
    TEST_RUN(test_error_recovery_direct_fallback);
    
    TEST_CLEANUP();
    return 0;
}