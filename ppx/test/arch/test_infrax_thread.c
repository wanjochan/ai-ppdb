#include <stdio.h>
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxCore.h"
#include <string.h>

// Get singleton instance of InfraxCore
static InfraxCore* core = NULL;

static void* test_thread_func(void* arg) {
    int* value = (int*)arg;
    if (value) {
        (*value)++;
    }
    return value;
}

void test_thread_basic(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    printf("Testing basic thread operations...\n");
    
    int test_value = 0;
    InfraxThreadConfig config = {
        .name = "test_thread",
        .entry_point = test_thread_func,
        .arg = &test_value
    };
    
    // Create thread instance
    InfraxThread* thread = InfraxThreadClass.new(&config);
    if (thread == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread != NULL", "Failed to create thread");
    }
    
    // Check initial thread state
    if (thread->is_running) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "!thread->is_running", "Thread should not be running initially");
    }
    
    // Start thread
    InfraxError err = thread->start(thread);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    // Check thread state after start
    if (!thread->is_running) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread->is_running", "Thread should be running after start");
    }
    
    // Get thread ID
    InfraxThreadId tid = thread->tid(thread);
    if (tid == 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "tid != 0", "Failed to get thread ID");
    }
    
    // Join thread
    void* result;
    err = thread->join(thread, &result);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    
    // Check thread state after join
    if (thread->is_running) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "!thread->is_running", "Thread should not be running after join");
    }
    
    // Check result
    if (test_value != 1) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "test_value == 1", "Thread function did not execute properly");
    }
    if (*(int*)result != 1) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "*(int*)result == 1", "Thread return value is incorrect");
    }
    
    // Clean up
    InfraxThreadClass.free(thread);
    
    printf("Basic thread test passed\n");
}

void test_thread_multiple(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    printf("Testing multiple threads...\n");
    
    #define NUM_THREADS 5
    int test_values[NUM_THREADS] = {0};
    InfraxThread* threads[NUM_THREADS];
    
    // Create and start threads
    for (int i = 0; i < NUM_THREADS; i++) {
        InfraxThreadConfig config = {
            .name = "test_thread",
            .entry_point = test_thread_func,
            .arg = &test_values[i]
        };
        
        threads[i] = InfraxThreadClass.new(&config);
        if (threads[i] == NULL) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "threads[i] != NULL", "Failed to create thread");
        }
        
        InfraxError err = threads[i]->start(threads[i]);
        if (err.code != 0) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
        }
    }
    
    // Join threads and verify results
    for (int i = 0; i < NUM_THREADS; i++) {
        void* result;
        InfraxError err = threads[i]->join(threads[i], &result);
        if (err.code != 0) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
        }
        if (test_values[i] != 1) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "test_values[i] == 1", "Thread function did not execute properly");
        }
        if (*(int*)result != 1) {
            core->assert_failed(core, __FILE__, __LINE__, __func__, "*(int*)result == 1", "Thread return value is incorrect");
        }
        
        InfraxThreadClass.free(threads[i]);
    }
    
    printf("Multiple threads test passed\n");
}

void test_thread_error_handling(void) {
    if (!core) core = InfraxCoreClass.singleton();
    
    printf("Testing thread error handling...\n");
    
    // Test invalid config
    InfraxThreadConfig invalid_config = {
        .name = NULL,
        .entry_point = NULL,
        .arg = NULL
    };
    
    InfraxThread* thread = InfraxThreadClass.new(&invalid_config);
    if (thread != NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread == NULL", "Thread creation with invalid config should fail");
    }
    
    // Test starting a thread with valid config but NULL entry point
    InfraxThreadConfig null_entry_config = {
        .name = "test_thread",
        .entry_point = NULL,
        .arg = NULL
    };
    
    thread = InfraxThreadClass.new(&null_entry_config);
    if (thread != NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread == NULL", "Thread creation with NULL entry point should fail");
    }
    
    // Test starting a thread with valid config but NULL name
    InfraxThreadConfig null_name_config = {
        .name = NULL,
        .entry_point = test_thread_func,
        .arg = NULL
    };
    
    thread = InfraxThreadClass.new(&null_name_config);
    if (thread != NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread == NULL", "Thread creation with NULL name should fail");
    }
    
    // Test starting a valid thread twice
    InfraxThreadConfig valid_config = {
        .name = "test_thread",
        .entry_point = test_thread_func,
        .arg = NULL
    };
    
    thread = InfraxThreadClass.new(&valid_config);
    if (thread == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "thread != NULL", "Thread creation with valid config should succeed");
    }
    
    InfraxError err = thread->start(thread);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", "First start should succeed");
    }
    
    err = thread->start(thread);
    if (err.code == 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code != 0", "Second start should fail");
    }
    
    // Clean up
    void* result;
    err = thread->join(thread, &result);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", "Join should succeed");
    }
    
    InfraxThreadClass.free(thread);
    
    printf("Thread error handling test passed\n");
}

int main(void) {
    printf("===================\nStarting InfraxThread tests...\n");
    
    test_thread_basic();
    test_thread_multiple();
    test_thread_error_handling();
    
    printf("All InfraxThread tests passed!\n");
    return 0;
}