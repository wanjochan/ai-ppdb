#include <cosmopolitan.h>
#include "internal/base.h"

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

int main(void) {
    printf("Testing basic error handling...\n");
    TEST_RUN(test_error_basic);
    printf("PASSED\n");
    
    printf("Testing error stack...\n");
    TEST_RUN(test_error_stack);
    printf("PASSED\n");
    
    printf("Testing error callback...\n");
    TEST_RUN(test_error_callback_func);
    printf("PASSED\n");
    
    printf("Testing error statistics...\n");
    TEST_RUN(test_error_stats);
    printf("PASSED\n");
    
    printf("Testing concurrent error handling...\n");
    TEST_RUN(test_error_concurrent);
    printf("PASSED\n");
    
    return 0;
}