#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxLog.h"

// Test state
typedef struct {
    int value;
    InfraxAsync* co;
} TestState;

static TestState* create_test_state(void) {
    TestState* state = (TestState*)malloc(sizeof(TestState));
    if (state) {
        state->value = 0;
        state->co = NULL;
    }
    return state;
}

static void free_test_state(TestState* state) {
    if (state) {
        free(state);
    }
}

static void test_coroutine_func(void* arg) {
    TestState* state = (TestState*)arg;
    if (state) {
        state->value++;
        state->co->yield(state->co);
        state->value++;
    }
}

void test_async_basic(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Testing basic coroutine operations");
    
    TestState* state = create_test_state();
    if (!state) {
        log->error(log, "Failed to create test state");
        return;
    }
    
    InfraxAsyncConfig config = {
        .name = "test_coroutine",
        .fn = test_coroutine_func,
        .arg = state
    };
    
    // Create coroutine
    InfraxAsync* co = InfraxAsync_CLASS.new(&config);
    if (!co) {
        log->error(log, "Failed to create coroutine");
        free_test_state(state);
        return;
    }
    state->co = co;

    // Verify initial state
    if (co->is_done(co)) {
        log->error(log, "Coroutine should not be done initially");
        goto cleanup;
    }
    
    // Start coroutine
    InfraxError err = co->start(co);
    if (!INFRAX_ERROR_IS_OK(err)) {
        log->error(log, "Failed to start coroutine");
        goto cleanup;
    }
    
    // Run coroutine
    InfraxAsyncRun();
    
    if (state->value != 1) {
        log->error(log, "First increment failed");
        goto cleanup;
    }
    
    // Resume coroutine
    err = co->resume(co);
    if (!INFRAX_ERROR_IS_OK(err)) {
        log->error(log, "Failed to resume coroutine");
        goto cleanup;
    }
    
    // Run coroutine again
    InfraxAsyncRun();
    
    if (state->value != 2) {
        log->error(log, "Second increment failed");
        goto cleanup;
    }
    
    if (!co->is_done(co)) {
        log->error(log, "Coroutine should be done");
        goto cleanup;
    }
    
    log->debug(log, "Basic coroutine test passed");

cleanup:
    if (co) {
        InfraxAsync_CLASS.free(co);
    }
    free_test_state(state);
}

void test_async_multiple(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Testing multiple coroutines");
    
    const int NUM_COROUTINES = 5;
    InfraxAsync** coroutines = malloc(NUM_COROUTINES * sizeof(InfraxAsync*));
    TestState** states = malloc(NUM_COROUTINES * sizeof(TestState*));
    
    if (!coroutines || !states) {
        log->error(log, "Failed to allocate arrays");
        free(coroutines);
        free(states);
        return;
    }

    memset(coroutines, 0, NUM_COROUTINES * sizeof(InfraxAsync*));
    memset(states, 0, NUM_COROUTINES * sizeof(TestState*));
    
    // Create states and coroutines
    for (int i = 0; i < NUM_COROUTINES; i++) {
        states[i] = create_test_state();
        if (!states[i]) {
            log->error(log, "Failed to create test state");
            goto cleanup;
        }
        
        char name[32];
        snprintf(name, sizeof(name), "test_coroutine_%d", i);
        InfraxAsyncConfig config = {
            .name = name,
            .fn = test_coroutine_func,
            .arg = states[i]
        };
        
        coroutines[i] = InfraxAsync_CLASS.new(&config);
        if (!coroutines[i]) {
            log->error(log, "Failed to create coroutine");
            goto cleanup;
        }
        states[i]->co = coroutines[i];
    }
    
    // Start all coroutines
    for (int i = 0; i < NUM_COROUTINES; i++) {
        InfraxError err = coroutines[i]->start(coroutines[i]);
        if (!INFRAX_ERROR_IS_OK(err)) {
            log->error(log, "Failed to start coroutine");
            goto cleanup;
        }
    }
    
    // Run all coroutines first time
    InfraxAsyncRun();
    
    // Verify first increment
    for (int i = 0; i < NUM_COROUTINES; i++) {
        if (states[i]->value != 1) {
            log->error(log, "First increment failed");
            goto cleanup;
        }
    }
    
    // Resume all coroutines
    for (int i = 0; i < NUM_COROUTINES; i++) {
        InfraxError err = coroutines[i]->resume(coroutines[i]);
        if (!INFRAX_ERROR_IS_OK(err)) {
            log->error(log, "Failed to resume coroutine");
            goto cleanup;
        }
    }
    
    // Run all coroutines second time
    InfraxAsyncRun();
    
    // Verify second increment and completion
    for (int i = 0; i < NUM_COROUTINES; i++) {
        if (states[i]->value != 2) {
            log->error(log, "Second increment failed");
            goto cleanup;
        }
        if (!coroutines[i]->is_done(coroutines[i])) {
            log->error(log, "Coroutine should be done");
            goto cleanup;
        }
    }
    
    log->debug(log, "Multiple coroutines test passed");

cleanup:
    for (int i = 0; i < NUM_COROUTINES; i++) {
        if (coroutines[i]) {
            InfraxAsync_CLASS.free(coroutines[i]);
        }
        if (states[i]) {
            free_test_state(states[i]);
        }
    }
    free(coroutines);
    free(states);
}

void test_async_error_handling(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Testing error handling");
    
    // Test invalid start
    InfraxAsyncConfig config = {
        .name = "test_coroutine",
        .fn = NULL,  // Invalid function pointer
        .arg = NULL
    };
    
    InfraxAsync* co = InfraxAsync_CLASS.new(&config);
    if (co) {
        log->error(log, "Should not create coroutine with invalid config");
        InfraxAsync_CLASS.free(co);
        return;
    }
    
    // Test invalid yield
    TestState* state = create_test_state();
    if (!state) {
        log->error(log, "Failed to create test state");
        return;
    }
    
    config.fn = test_coroutine_func;
    config.arg = state;
    co = InfraxAsync_CLASS.new(&config);
    if (!co) {
        log->error(log, "Failed to create coroutine");
        free_test_state(state);
        return;
    }
    state->co = co;
    
    // Try to yield before starting
    InfraxError err = co->yield(co);
    if (INFRAX_ERROR_IS_OK(err)) {
        log->error(log, "Should not yield before starting");
        goto cleanup;
    }
    
    log->debug(log, "Error handling test passed");

cleanup:
    if (co) {
        InfraxAsync_CLASS.free(co);
    }
    free_test_state(state);
}

int main(void) {
    test_async_basic();
    test_async_multiple();
    test_async_error_handling();
    return 0;
}
