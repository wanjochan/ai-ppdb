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
    InfraxAsync* coroutines[NUM_COROUTINES] = {NULL};
    TestState* states[NUM_COROUTINES] = {NULL};
    
    // Create states
    for (int i = 0; i < NUM_COROUTINES; i++) {
        states[i] = create_test_state();
        if (!states[i]) {
            log->error(log, "Failed to create test state");
            goto cleanup;
        }
    }
    
    // Create coroutines
    for (int i = 0; i < NUM_COROUTINES; i++) {
        static char names[5][32];  // Static array to store names
        snprintf(names[i], sizeof(names[i]), "test_coroutine_%d", i);
        InfraxAsyncConfig config = {
            .name = names[i],
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
    for (int i = 0; i < NUM_COROUTINES; i++) {
        InfraxAsyncRun();
    }
    
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
    for (int i = 0; i < NUM_COROUTINES; i++) {
        InfraxAsyncRun();
    }
    
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
        free_test_state(states[i]);
    }
}

void test_async_error_handling(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Testing coroutine error handling");
    
    // Test invalid coroutine config
    InfraxAsyncConfig config = {
        .name = "error_test_coroutine",
        .fn = NULL,  // Invalid function pointer
        .arg = NULL
    };
    
    InfraxAsync* co = InfraxAsync_CLASS.new(&config);
    if (co) {
        log->error(log, "Should fail to create coroutine with NULL function");
        InfraxAsync_CLASS.free(co);
        return;
    }
    
    // Test double start
    config.fn = test_coroutine_func;
    co = InfraxAsync_CLASS.new(&config);
    if (!co) {
        log->error(log, "Failed to create coroutine");
        return;
    }
    
    InfraxError err = co->start(co);
    if (!INFRAX_ERROR_IS_OK(err)) {
        log->error(log, "First start should succeed");
        goto cleanup;
    }
    
    err = co->start(co);
    if (INFRAX_ERROR_IS_OK(err)) {
        log->error(log, "Second start should fail");
        goto cleanup;
    }
    
    // Test resume before start
    InfraxAsync* unstarted = InfraxAsync_CLASS.new(&config);
    if (!unstarted) {
        log->error(log, "Failed to create coroutine");
        goto cleanup;
    }
    
    err = unstarted->resume(unstarted);
    if (INFRAX_ERROR_IS_OK(err)) {
        log->error(log, "Resume before start should fail");
        InfraxAsync_CLASS.free(unstarted);
        goto cleanup;
    }
    
    InfraxAsync_CLASS.free(unstarted);
    log->debug(log, "Error handling test passed");

cleanup:
    if (co) {
        InfraxAsync_CLASS.free(co);
    }
}

int main(void) {
    test_async_basic();
    test_async_multiple();
    test_async_error_handling();
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "All coroutine tests passed");
    return 0;
}
