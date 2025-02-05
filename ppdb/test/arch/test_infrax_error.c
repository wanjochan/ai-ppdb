#include "cosmopolitan.h"
#include <assert.h>
#include <pthread.h>
#include "ppdb/PpxInfra.h"

// Test basic error operations
void test_error_operations(void) {
    InfraxCore* core = get_global_infrax_core();
    InfraxError error = core->new_error(0, NULL);
    
    // Test initial state
    assert(error.code == 0);
    assert(strlen(error.message) == 0);
    
    //not recommend but should work:
    InfraxError error2 = g_infrax_core.new_error(2, "error 2");
    assert(error2.code == 2);
    assert(strcmp(error2.message, "error 2") == 0);
    
    printf("Basic error operations test passed\n");
}

// Test new_error functionality
void test_new_error(void) {
    InfraxCore* core = get_global_infrax_core();
    assert(core != NULL);
    
    // Test creating new error
    InfraxError e1 = core->new_error(1, "Test error");
    assert(e1.code == 1);
    assert(strcmp(e1.message, "Test error") == 0);
    
    // Test message truncation
    char long_message[512];
    memset(long_message, 'A', sizeof(long_message));
    long_message[511] = '\0';
    
    InfraxError e2 = core->new_error(2, long_message);
    assert(e2.code == 2);
    assert(strlen(e2.message) == 127);  // Should be truncated
    assert(e2.message[127] == '\0');    // Should be null terminated
    
    // Test empty message
    InfraxError e3 = core->new_error(3, "");
    assert(e3.code == 3);
    assert(strlen(e3.message) == 0);
    
    // Test NULL message
    InfraxError e4 = core->new_error(4, NULL);
    assert(e4.code == 4);
    assert(strlen(e4.message) == 0);
    
    printf("New error functionality test passed\n");
}

// Test error value copying
void test_error_value_semantics(void) {
    InfraxCore* core = get_global_infrax_core();
    assert(core != NULL);
    
    // Test error value assignment
    InfraxError e1 = core->new_error(1, "Original error");
    InfraxError e2 = e1;  // Copy the error
    
    assert(e1.code == e2.code);
    assert(strcmp(e1.message, e2.message) == 0);
    
    // Modify e1 and verify e2 remains unchanged
    e1 = core->new_error(2, "Modified error");
    assert(e2.code == 1);
    assert(strcmp(e2.message, "Original error") == 0);
    
    printf("Error value semantics test passed\n");
}

// Thread function for testing thread local storage
void* thread_function(void* arg) {
    InfraxCore* core = get_global_infrax_core();
    assert(core != NULL);
    
    InfraxError error = core->new_error(-2, "Thread specific error");
    
    // Verify thread-specific error
    assert(error.code == -2);
    assert(strcmp(error.message, "Thread specific error") == 0);
    
    return NULL;
}

// Test thread safety
void test_thread_safety(void) {
    InfraxCore* core = get_global_infrax_core();
    assert(core != NULL);
    
    InfraxError main_error = core->new_error(-1, "Main thread error");
    
    pthread_t thread;
    pthread_create(&thread, NULL, thread_function, NULL);
    pthread_join(thread, NULL);
    
    // Verify main thread error remains unchanged
    assert(main_error.code == -1);
    assert(strcmp(main_error.message, "Main thread error") == 0);
    
    printf("Thread safety test passed\n");
}

// Test error handling in functions
InfraxError process_with_error(int value) {
    InfraxCore* core = get_global_infrax_core();
    assert(core != NULL);
    
    if (value < 0) {
        return core->new_error(-1, "Negative value not allowed");
    }
    if (value > 100) {
        return core->new_error(-2, "Value too large");
    }
    return core->new_error(0, "Success");
}

void test_error_handling(void) {
    // Test error cases
    InfraxError e1 = process_with_error(-5);
    assert(e1.code == -1);
    assert(strcmp(e1.message, "Negative value not allowed") == 0);
    
    InfraxError e2 = process_with_error(150);
    assert(e2.code == -2);
    assert(strcmp(e2.message, "Value too large") == 0);
    
    // Test success case
    InfraxError e3 = process_with_error(50);
    assert(e3.code == 0);
    assert(strcmp(e3.message, "Success") == 0);
    
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
