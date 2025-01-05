#include <cosmopolitan.h>
#include "internal/base.h"

// Test suite for error handling
static void test_error_basic(void) {
    ppdb_error_t err = PPDB_OK;
    assert(err == PPDB_OK);
}

static void test_error_context(void) {
    ppdb_error_context_t ctx;
    ctx.code = PPDB_ERR_MEMORY;
    ctx.file = __FILE__;
    ctx.line = __LINE__;
    ctx.func = __func__;
    snprintf(ctx.message, sizeof(ctx.message), "Test error message");
    
    ppdb_error_set_context(&ctx);
    const ppdb_error_context_t* get_ctx = ppdb_error_get_context();
    
    assert(get_ctx->code == ctx.code);
    assert(strcmp(get_ctx->file, ctx.file) == 0);
    assert(get_ctx->line == ctx.line);
    assert(strcmp(get_ctx->func, ctx.func) == 0);
    assert(strcmp(get_ctx->message, ctx.message) == 0);
}

static void test_error_string(void) {
    const char* ok_str = ppdb_error_to_string(PPDB_OK);
    assert(strcmp(ok_str, "Success") == 0);
    
    const char* memory_str = ppdb_error_to_string(PPDB_ERR_MEMORY);
    assert(strcmp(memory_str, "Memory allocation failed") == 0);
}

int main(void) {
    printf("Running test suite: Error Tests\n");
    
    printf("  Running test: test_error_basic\n");
    test_error_basic();
    printf("  Test passed: test_error_basic\n");
    
    printf("  Running test: test_error_context\n");
    test_error_context();
    printf("  Test passed: test_error_context\n");
    
    printf("  Running test: test_error_string\n");
    test_error_string();
    printf("  Test passed: test_error_string\n");
    
    printf("Test suite completed\n");
    return 0;
} 