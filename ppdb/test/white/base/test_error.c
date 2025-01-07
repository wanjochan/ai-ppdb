#include <cosmopolitan.h>
#include "internal/base.h"

// Test basic error operations
static void test_error_basic(void) {
    ppdb_error_t err;
    ppdb_base_error_context_t* ctx = NULL;

    // Create error context
    err = ppdb_base_error_context_create(&ctx);
    assert(err == PPDB_OK);
    assert(ctx != NULL);

    // Test error setting
    err = ppdb_base_error_set(ctx, PPDB_BASE_ERR_MEMORY, "Memory allocation failed");
    assert(err == PPDB_BASE_ERR_MEMORY);

    // Test error getting
    const char* error_msg = ppdb_base_error_get_message(ctx);
    assert(strcmp(error_msg, "Memory allocation failed") == 0);

    // Test error code getting
    ppdb_error_t error_code = ppdb_base_error_get_code(ctx);
    assert(error_code == PPDB_BASE_ERR_MEMORY);

    // Cleanup
    ppdb_base_error_context_destroy(ctx);
}

// Test error formatting
static void test_error_format(void) {
    ppdb_error_t err;
    ppdb_base_error_context_t* ctx = NULL;

    // Create error context
    err = ppdb_base_error_context_create(&ctx);
    assert(err == PPDB_OK);

    // Test error formatting
    err = ppdb_base_error_setf(ctx, PPDB_BASE_ERR_IO, "IO error: %s at offset %d", "read failed", 1024);
    assert(err == PPDB_BASE_ERR_IO);

    // Verify formatted message
    const char* error_msg = ppdb_base_error_get_message(ctx);
    assert(strstr(error_msg, "IO error: read failed at offset 1024") != NULL);

    // Cleanup
    ppdb_base_error_context_destroy(ctx);
}

// Test error statistics
static void test_error_stats(void) {
    ppdb_error_t err;
    ppdb_base_error_context_t* ctx = NULL;
    ppdb_base_error_stats_t stats;

    // Create error context
    err = ppdb_base_error_context_create(&ctx);
    assert(err == PPDB_OK);

    // Check initial stats
    ppdb_base_error_get_stats(ctx, &stats);
    assert(stats.total_errors == 0);
    assert(stats.memory_errors == 0);
    assert(stats.io_errors == 0);

    // Generate some errors
    ppdb_base_error_set(ctx, PPDB_BASE_ERR_MEMORY, "Memory error 1");
    ppdb_base_error_set(ctx, PPDB_BASE_ERR_MEMORY, "Memory error 2");
    ppdb_base_error_set(ctx, PPDB_BASE_ERR_IO, "IO error 1");

    // Check updated stats
    ppdb_base_error_get_stats(ctx, &stats);
    assert(stats.total_errors == 3);
    assert(stats.memory_errors == 2);
    assert(stats.io_errors == 1);

    // Cleanup
    ppdb_base_error_context_destroy(ctx);
}

// Test error handling edge cases
static void test_error_edge_cases(void) {
    ppdb_error_t err;
    ppdb_base_error_context_t* ctx = NULL;

    // Test invalid parameters
    err = ppdb_base_error_context_create(NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_error_context_create(&ctx);
    assert(err == PPDB_OK);

    // Test NULL message
    err = ppdb_base_error_set(ctx, PPDB_BASE_ERR_MEMORY, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Test empty message
    err = ppdb_base_error_set(ctx, PPDB_BASE_ERR_MEMORY, "");
    assert(err == PPDB_BASE_ERR_PARAM);

    // Test invalid error code
    err = ppdb_base_error_set(ctx, -1, "Invalid error");
    assert(err == PPDB_BASE_ERR_PARAM);

    // Cleanup
    ppdb_base_error_context_destroy(ctx);
}

int main(void) {
    printf("Testing error basic operations...\n");
    test_error_basic();
    printf("PASSED\n");

    printf("Testing error formatting...\n");
    test_error_format();
    printf("PASSED\n");

    printf("Testing error statistics...\n");
    test_error_stats();
    printf("PASSED\n");

    printf("Testing error edge cases...\n");
    test_error_edge_cases();
    printf("PASSED\n");

    return 0;
}