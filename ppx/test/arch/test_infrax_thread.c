#include <stdio.h>
#include <assert.h>
#include "internal/infrax/InfraxThread.h"
#include <string.h>

static void* test_thread_func(void* arg) {
    int* value = (int*)arg;
    if (value) {
        (*value)++;
    }
    return value;
}

void test_thread_basic(void) {
    printf("Testing basic thread operations...\n");
    
    int test_value = 0;
    InfraxThreadConfig config = {
        .name = "test_thread",
        .entry_point = test_thread_func,
        .arg = &test_value
    };
    
    // Create thread
    InfraxThread* thread = InfraxThread_CLASS.new(&config);
    assert(thread != NULL);
    assert(thread->klass == &InfraxThread_CLASS);
    
    // Start thread
    InfraxError err = thread->start(thread);
    assert(err.code == 0);
    
    // Get thread ID
    InfraxThreadId tid = thread->tid(thread);
    assert(tid != 0);
    
    // Join thread
    void* result;
    err = thread->join(thread, &result);
    assert(err.code == 0);
    assert(test_value == 1);
    assert(*(int*)result == 1);
    
    // Cleanup
    InfraxThread_CLASS.free(thread);
    
    printf("Basic thread test passed\n");
}

void test_thread_multiple(void) {
    printf("Testing multiple threads...\n");
    
    const int NUM_THREADS = 5;
    InfraxThread* threads[NUM_THREADS];
    int values[NUM_THREADS];
    memset(values, 0, sizeof(values));
    
    // Create and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "thread_%d", i);
        
        InfraxThreadConfig config = {
            .name = name,
            .entry_point = test_thread_func,
            .arg = &values[i]
        };
        
        threads[i] = InfraxThread_CLASS.new(&config);
        assert(threads[i] != NULL);
        
        InfraxError err = threads[i]->start(threads[i]);
        assert(err.code == 0);
        
        InfraxThreadId tid = threads[i]->tid(threads[i]);
        assert(tid != 0);
    }
    
    // Join threads
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result;
        InfraxError err = threads[i]->join(threads[i], &result);
        assert(err.code == 0);
        assert(values[i] == 1);
        assert(*(int*)result == 1);
        
        InfraxThread_CLASS.free(threads[i]);
    }
    
    printf("Multiple threads test passed\n");
}

void test_thread_error_handling(void) {
    printf("Testing thread error handling...\n");
    
    // Test invalid thread
    InfraxThreadConfig config = {
        .name = "error_test_thread",
        .entry_point = test_thread_func,
        .arg = NULL
    };
    
    InfraxThread* thread = InfraxThread_CLASS.new(&config);
    assert(thread != NULL);
    
    // Test double start
    InfraxError err = thread->start(thread);
    assert(err.code == 0);
    
    err = thread->start(thread);
    assert(err.code == INFRAX_ERROR_INVALID_ARGUMENT);
    
    // Test join
    void* result;
    err = thread->join(thread, &result);
    assert(err.code == 0);
    assert(result == NULL);  // 因为我们传入了 NULL 作为 arg
    
    // Cleanup
    InfraxThread_CLASS.free(thread);
    
    printf("Thread error handling test passed\n");
}

int main(void) {
    printf("Starting InfraxThread tests...\n");
    
    test_thread_basic();
    test_thread_multiple();
    test_thread_error_handling();
    
    printf("All InfraxThread tests passed!\n");
    return 0;
} 