#include <cosmopolitan.h>
#include "internal/base.h"

// Test suite for error handling
void test_error_basic(void) {
    assert(PPDB_ERR_MEMORY != PPDB_OK);
    assert(ppdb_base_error_init() == PPDB_OK);
}

void test_error_context(void) {
    ppdb_error_context_t ctx = {0};
    ctx.code = PPDB_ERR_MEMORY;
    ctx.file = __FILE__;
    ctx.line = __LINE__;
    ctx.func = __func__;
    strncpy(ctx.message, "Test error", PPDB_MAX_ERROR_MESSAGE - 1);
    
    assert(ppdb_base_error_set_context(&ctx) == PPDB_OK);
    
    const ppdb_error_context_t* get_ctx = ppdb_base_error_get_context();
    assert(get_ctx != NULL);
    assert(get_ctx->code == ctx.code);
    assert(strcmp(get_ctx->file, ctx.file) == 0);
    assert(get_ctx->line == ctx.line);
    assert(strcmp(get_ctx->func, ctx.func) == 0);
    assert(strcmp(get_ctx->message, ctx.message) == 0);
}

void test_error_string(void) {
    assert(strcmp(ppdb_base_error_to_string(PPDB_ERR_MEMORY), "Memory allocation failed") == 0);
    assert(strcmp(ppdb_base_error_to_string(PPDB_OK), "Success") == 0);
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
