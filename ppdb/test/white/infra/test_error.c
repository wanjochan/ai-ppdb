#include <cosmopolitan.h>
#include "internal/base.h"

// Test suite for error handling
static void test_error_basic(void) {
    assert(PPDB_BASE_ERR_MEMORY != PPDB_OK);
}

static void test_error_context(void) {
    ppdb_error_context_t ctx;
    ctx.code = PPDB_BASE_ERR_MEMORY;
    ctx.file = __FILE__;
    ctx.line = __LINE__;
    ctx.func = __func__;
    snprintf(ctx.message, sizeof(ctx.message), "Test error message");
    
    ppdb_error_set_context(&ctx);
    
    assert(ppdb_error_get_context()->code == ctx.code);
    assert(strcmp(ppdb_error_get_context()->file, ctx.file) == 0);
    assert(ppdb_error_get_context()->line == ctx.line);
    assert(strcmp(ppdb_error_get_context()->func, ctx.func) == 0);
    assert(strcmp(ppdb_error_get_context()->message, ctx.message) == 0);
}

static void test_error_string(void) {
    assert(strcmp(ppdb_error_to_string(PPDB_OK), "Success") == 0);
    assert(strcmp(ppdb_error_to_string(PPDB_BASE_ERR_MEMORY), "Memory error") == 0);
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
