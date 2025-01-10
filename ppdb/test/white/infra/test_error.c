#include "test/test_common.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_error.h"
#include "test/test_framework.h"

// Basic functionality tests
static void test_error_basic(void) {
    TEST_ASSERT(PPDB_ERR_MEMORY != PPDB_OK);
    TEST_ASSERT(ppdb_base_error_init() == PPDB_OK);
    TEST_ASSERT(ppdb_base_error_get_context() != NULL);
}

// Error context handling tests
static void test_error_context(void) {
    ppdb_error_context_t ctx = {0};
    ctx.code = PPDB_ERR_MEMORY;
    ctx.file = __FILE__;
    ctx.line = __LINE__;
    ctx.func = __func__;
    strncpy(ctx.message, "Test error", PPDB_MAX_ERROR_MESSAGE - 1);
    
    TEST_ASSERT(ppdb_base_error_set_context(&ctx) == PPDB_OK);
    
    const ppdb_error_context_t* get_ctx = ppdb_base_error_get_context();
    TEST_ASSERT(get_ctx != NULL);
    TEST_ASSERT(get_ctx->code == ctx.code);
    TEST_ASSERT(strcmp(get_ctx->file, ctx.file) == 0);
    TEST_ASSERT(get_ctx->line == ctx.line);
    TEST_ASSERT(strcmp(get_ctx->func, ctx.func) == 0);
    TEST_ASSERT(strcmp(get_ctx->message, ctx.message) == 0);
}

// Boundary condition tests
static void test_error_boundary(void) {
    ppdb_error_context_t ctx = {0};
    char long_message[PPDB_MAX_ERROR_MESSAGE * 2] = {0};
    memset(long_message, 'A', PPDB_MAX_ERROR_MESSAGE * 2 - 1);
    
    ctx.code = PPDB_ERR_MEMORY;
    strncpy(ctx.message, long_message, PPDB_MAX_ERROR_MESSAGE - 1);
    TEST_ASSERT(ppdb_base_error_set_context(&ctx) == PPDB_OK);
    TEST_ASSERT(strlen(ppdb_base_error_get_context()->message) < PPDB_MAX_ERROR_MESSAGE);
}

// Performance test
static void test_error_performance(void) {
    int64_t start = ppdb_time_now();
    for(int i = 0; i < 10000; i++) {
        ppdb_error_context_t ctx = {0};
        ctx.code = PPDB_ERR_MEMORY;
        ppdb_base_error_set_context(&ctx);
        ppdb_base_error_get_context();
    }
    int64_t end = ppdb_time_now();
    double time_spent = (end - start) / 1000000.0;
    TEST_ASSERT(time_spent < 1.0); // Should complete within 1 second
}

// Thread safety test
static void* concurrent_error_test(void* arg) {
    for(int i = 0; i < 1000; i++) {
        ppdb_error_context_t ctx = {0};
        ctx.code = PPDB_ERR_MEMORY;
        TEST_ASSERT(ppdb_base_error_set_context(&ctx) == PPDB_OK);
        TEST_ASSERT(ppdb_base_error_get_context() != NULL);
    }
    return NULL;
}

static void test_error_concurrent(void) {
    ppdb_thread_t* threads[10];
    ppdb_error_t err;

    for(int i = 0; i < 10; i++) {
        err = ppdb_thread_create(&threads[i], concurrent_error_test, NULL);
        TEST_ASSERT(err == PPDB_OK, "Thread creation failed");
    }

    for(int i = 0; i < 10; i++) {
        err = ppdb_thread_join(threads[i]);
        TEST_ASSERT(err == PPDB_OK, "Thread join failed");
    }
}

int main(void) {
    printf("Running comprehensive error test suite\n");
    
    RUN_TEST(test_error_basic);
    RUN_TEST(test_error_context);
    RUN_TEST(test_error_boundary);
    RUN_TEST(test_error_performance);
    RUN_TEST(test_error_concurrent);
    
    printf("All tests completed successfully\n");
    return 0;
}
