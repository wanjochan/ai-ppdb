#include "cosmopolitan.h"
#include <assert.h>
#include <pthread.h>
#include "internal/infrax/InfraxCore.h"

// Test basic error operations
void test_error_operations(void) {
    InfraxError error = INFRAX_ERROR_OK_STRUCT;
    
    // Test initial state
    assert(INFRAX_ERROR_IS_OK(error));
    assert(strlen(error.message) == 0);
    
    //not recommend but should work:
    InfraxError error2 = make_error(INFRAX_ERROR_INVALID_PARAM, "error 2");
    assert(!INFRAX_ERROR_IS_OK(error2));
    assert(strcmp(error2.message, "error 2") == 0);
    
    printf("Basic error operations test passed\n");
}

// Test new_error functionality
void test_new_error(void) {
    // Test creating new error
    InfraxError e1 = make_error(INFRAX_ERROR_INVALID_PARAM, "Test error");
    assert(!INFRAX_ERROR_IS_OK(e1));
    assert(strcmp(e1.message, "Test error") == 0);
    
    // Test message truncation
    char long_message[512];
    memset(long_message, 'A', sizeof(long_message));
    long_message[511] = '\0';
    
    InfraxError e2 = make_error(INFRAX_ERROR_NO_MEMORY, long_message);
    assert(!INFRAX_ERROR_IS_OK(e2));
    assert(strlen(e2.message) == 127);  // Should be truncated
    assert(e2.message[127] == '\0');    // Should be null terminated
    
    // Test empty message
    InfraxError e3 = make_error(INFRAX_ERROR_INVALID_PARAM, "");
    assert(!INFRAX_ERROR_IS_OK(e3));
    assert(strlen(e3.message) == 0);
    
    // Test NULL message
    InfraxError e4 = INFRAX_ERROR_OK_STRUCT;
    assert(INFRAX_ERROR_IS_OK(e4));
    assert(strlen(e4.message) == 0);
    
    printf("New error functionality test passed\n");
}

// Test error value semantics
void test_error_value_semantics(void) {
    // Test error value assignment
    InfraxError e1 = make_error(INFRAX_ERROR_INVALID_PARAM, "Original error");
    InfraxError e2 = e1;  // Copy the error
    
    assert(!INFRAX_ERROR_IS_OK(e1));
    assert(!INFRAX_ERROR_IS_OK(e2));
    assert(strcmp(e1.message, e2.message) == 0);
    
    // Modify e1 and verify e2 remains unchanged
    e1 = make_error(INFRAX_ERROR_NO_MEMORY, "Modified error");
    assert(!INFRAX_ERROR_IS_OK(e2));
    assert(strcmp(e2.message, "Original error") == 0);
    
    printf("Error value semantics test passed\n");
}

// Thread function for testing thread local storage
void* thread_function(void* arg) {
    InfraxError error = make_error(INFRAX_ERROR_INVALID_PARAM, "Thread specific error");
    
    // Verify thread-specific error
    assert(!INFRAX_ERROR_IS_OK(error));
    assert(strcmp(error.message, "Thread specific error") == 0);
    
    return NULL;
}

// Test thread safety
void test_thread_safety(void) {
    InfraxError main_error = make_error(INFRAX_ERROR_INVALID_PARAM, "Main thread error");
    
    pthread_t thread;
    pthread_create(&thread, NULL, thread_function, NULL);
    pthread_join(thread, NULL);
    
    // Verify main thread error remains unchanged
    assert(!INFRAX_ERROR_IS_OK(main_error));
    assert(strcmp(main_error.message, "Main thread error") == 0);
    
    printf("Thread safety test passed\n");
}

// Test error handling in functions
InfraxError process_with_error(int value) {
    if (value < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Negative value not allowed");
    }
    
    if (value > 100) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Value too large");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

void test_error_handling(void) {
    InfraxError error;
    
    // Test negative value
    error = process_with_error(-5);
    assert(!INFRAX_ERROR_IS_OK(error));
    assert(strcmp(error.message, "Negative value not allowed") == 0);
    
    // Test too large value
    error = process_with_error(150);
    assert(!INFRAX_ERROR_IS_OK(error));
    assert(strcmp(error.message, "Value too large") == 0);
    
    // Test valid value
    error = process_with_error(50);
    assert(INFRAX_ERROR_IS_OK(error));
    assert(strlen(error.message) == 0);
    
    printf("Error handling test passed\n");
}

int main(void) {
    printf("Starting InfraxError tests...\n");
    
    test_error_operations();
    test_new_error();
    test_error_value_semantics();
    test_thread_safety();
    test_error_handling();
    
    printf("All InfraxError tests passed!\n");
    return 0;
}
