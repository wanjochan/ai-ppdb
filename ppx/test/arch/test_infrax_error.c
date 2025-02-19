// #include "cosmopolitan.h"
// #include <stdio.h>
// #include <string.h>
// #include <pthread.h>
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxThread.h"

InfraxCore* core = NULL;
// Test basic error operations
void test_error_operations(void) {
    core->printf(core, "Testing basic error operations...\n");
    InfraxError error = INFRAX_ERROR_OK_STRUCT;
    
    // Test initial state
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(error));
    INFRAX_ASSERT(core, core->strlen(core, error.message) == 0);
    
    // Not recommended but should work:
    InfraxError error2 = make_error(INFRAX_ERROR_INVALID_PARAM, "error 2");
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(error2));
    INFRAX_ASSERT(core, core->strcmp(core, error2.message, "error 2") == 0);
    
    core->printf(core, "Basic error operations test passed\n");
}

// Test error creation
void test_new_error(void) {
    core->printf(core, "Testing error creation...\n");
    
    // Test creating new error
    InfraxError e1 = make_error(INFRAX_ERROR_INVALID_PARAM, "Test error");
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(e1));
    INFRAX_ASSERT(core, core->strcmp(core, e1.message, "Test error") == 0);
    
    // Test message truncation
    char long_message[512];
    core->memset(core, long_message, 'A', sizeof(long_message) - 1);
    long_message[511] = '\0';
    
    InfraxError e2 = make_error(INFRAX_ERROR_NO_MEMORY, long_message);
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(e2));
    INFRAX_ASSERT(core, core->strlen(core, e2.message) == 127);  // Should be truncated
    INFRAX_ASSERT(core, e2.message[127] == '\0');    // Should be null terminated
    
    // Test empty message
    InfraxError e3 = make_error(INFRAX_ERROR_INVALID_PARAM, "");
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(e3));
    INFRAX_ASSERT(core, core->strlen(core, e3.message) == 0);
    
    // Test NULL message
    InfraxError e4 = INFRAX_ERROR_OK_STRUCT;
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(e4));
    INFRAX_ASSERT(core, core->strlen(core, e4.message) == 0);
    
    core->printf(core, "Error creation test passed\n");
}

// Test error value semantics
void test_error_value_semantics(void) {
    core->printf(core, "Testing error value semantics...\n");
    
    InfraxError e1 = make_error(INFRAX_ERROR_INVALID_PARAM, "Original error");
    InfraxError e2 = e1;  // Copy the error
    
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(e1));
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(e2));
    INFRAX_ASSERT(core, core->strcmp(core, e1.message, e2.message) == 0);
    
    // Modify e1 and verify e2 remains unchanged
    e1 = make_error(INFRAX_ERROR_NO_MEMORY, "Modified error");
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(e2));
    INFRAX_ASSERT(core, core->strcmp(core, e2.message, "Original error") == 0);
    
    core->printf(core, "Error value semantics test passed\n");
}

// Thread function for testing thread safety
static void* thread_func(void* arg) {
    (void)arg;  // Unused parameter
    InfraxError error = make_error(INFRAX_ERROR_INVALID_PARAM, "Thread specific error");
    
    // Verify thread-specific error
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(error));
    INFRAX_ASSERT(core, core->strcmp(core, error.message, "Thread specific error") == 0);
    
    return NULL;
}

// Test thread safety
void test_thread_safety(void) {
    core->printf(core, "Testing thread safety...\n");
    
    // 使用 InfraxThread 而不是直接使用 pthread
    InfraxThreadConfig config = {
        .name = "error_test_thread",
        .func = thread_func,
        .arg = NULL,
        .stack_size = 0,
        .priority = 0
    };
    
    InfraxThread* thread = InfraxThreadClass.new(&config);
    INFRAX_ASSERT(core, thread != NULL);
    
    InfraxError main_error = make_error(INFRAX_ERROR_INVALID_PARAM, "Main thread error");
    
    InfraxError err = InfraxThreadClass.start(thread, thread_func, NULL);
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_ERR(err));
    
    void* result;
    err = InfraxThreadClass.join(thread, &result);
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_ERR(err));
    
    // Verify main thread error remains unchanged
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(main_error));
    INFRAX_ASSERT(core, core->strcmp(core, main_error.message, "Main thread error") == 0);
    
    InfraxThreadClass.free(thread);
    core->printf(core, "Thread safety test passed\n");
}

// Helper function for testing error handling
InfraxError process_with_error(int value) {
    if (value < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Negative value not allowed");
    }
    if (value > 100) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Value too large");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

// Test error handling
void test_error_handling(void) {
    core->printf(core, "Testing error handling...\n");
    
    InfraxError error;
    
    // Test negative value
    error = process_with_error(-5);
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(error));
    INFRAX_ASSERT(core, core->strcmp(core, error.message, "Negative value not allowed") == 0);
    
    // Test too large value
    error = process_with_error(150);
    INFRAX_ASSERT(core, !INFRAX_ERROR_IS_OK(error));
    INFRAX_ASSERT(core, core->strcmp(core, error.message, "Value too large") == 0);
    
    // Test valid value
    error = process_with_error(50);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(error));
    INFRAX_ASSERT(core, core->strlen(core, error.message) == 0);
    
    core->printf(core, "Error handling test passed\n");
}

int main(void) {
    core = InfraxCoreClass.singleton();
    core->printf(core, "===================\nStarting InfraxError tests...\n");
    
    test_error_operations();
    test_new_error();
    test_error_value_semantics();
    test_thread_safety();
    test_error_handling();
    
    core->printf(core, "All infrax_error tests passed!\n===================\n");
    return 0;
}
