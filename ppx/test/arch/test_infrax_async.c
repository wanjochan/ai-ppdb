#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxLog.h"
#include <string.h>

#define MAX_COROUTINES 5

// Test state
typedef struct {
    int value;
    InfraxAsync* co;
} TestState;

static void test_coroutine_func(void* arg) {
    TestState* state = (TestState*)arg;
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Coroutine function started");
    
    if (state) {
        state->value++;
        log->debug(log, "First increment done, yielding");
        
        // 创建一个定时器，等待1毫秒
        InfraxAsync* timer = InfraxAsync_CreateTimer(1);
        if (timer) {
            // 启动定时器并等待它完成
            InfraxError err = timer->start(timer);
            if (INFRAX_ERROR_IS_OK(err)) {
                // 运行定时器直到完成
                while (!timer->is_done(timer)) {
                    InfraxAsyncRun();  // 运行调度器
                }
            }
            // 确保定时器完成后再释放
            while (!timer->is_done(timer)) {
                InfraxAsyncRun();
            }
            InfraxAsync_CLASS.free(timer);
        }
        
        log->debug(log, "Resumed after yield");
        state->value++;
        log->debug(log, "Second increment done");
    }
    log->debug(log, "Coroutine function finished");
}

void test_async_basic(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Testing basic coroutine operations");
    
    // 分配测试状态
    TestState* state = malloc(sizeof(TestState));
    if (!state) {
        log->error(log, "Failed to allocate test state");
        return;
    }
    memset(state, 0, sizeof(TestState));
    
    InfraxAsyncConfig config = {
        .name = "test_coroutine",
        .fn = test_coroutine_func,
        .arg = state,
        .stack_size = DEFAULT_STACK_SIZE  // 使用默认栈大小
    };
    
    // Create coroutine
    InfraxAsync* co = InfraxAsync_CLASS.new(&config);
    if (!co) {
        log->error(log, "Failed to create coroutine");
        goto cleanup;
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
    
    log->debug(log, "Running coroutine first time");
    InfraxAsyncRun();
    
    if (state->value != 1) {
        log->error(log, "First increment failed, value = %d", state->value);
        goto cleanup;
    }
    
    // Resume coroutine
    log->debug(log, "Resuming coroutine");
    err = co->resume(co);
    if (!INFRAX_ERROR_IS_OK(err)) {
        log->error(log, "Failed to resume coroutine");
        goto cleanup;
    }
    
    log->debug(log, "Running coroutine second time");
    InfraxAsyncRun();
    
    if (state->value != 2) {
        log->error(log, "Second increment failed, value = %d", state->value);
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
    free(state);
}

void test_async_multiple(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Testing multiple coroutines");
    
    TestState* states[MAX_COROUTINES] = {0};
    InfraxAsync* coroutines[MAX_COROUTINES] = {0};
    
    // Create coroutines
    for (int i = 0; i < MAX_COROUTINES; i++) {
        // 分配测试状态
        states[i] = malloc(sizeof(TestState));
        if (!states[i]) {
            log->error(log, "Failed to allocate test state %d", i);
            goto cleanup;
        }
        memset(states[i], 0, sizeof(TestState));
        
        char name[32];
        snprintf(name, sizeof(name), "test_coroutine_%d", i);
        InfraxAsyncConfig config = {
            .name = name,
            .fn = test_coroutine_func,
            .arg = states[i]
        };
        
        coroutines[i] = InfraxAsync_CLASS.new(&config);
        if (!coroutines[i]) {
            log->error(log, "Failed to create coroutine %d", i);
            goto cleanup;
        }
        states[i]->co = coroutines[i];
    }
    
    // Start all coroutines
    for (int i = 0; i < MAX_COROUTINES; i++) {
        InfraxError err = coroutines[i]->start(coroutines[i]);
        if (!INFRAX_ERROR_IS_OK(err)) {
            log->error(log, "Failed to start coroutine %d", i);
            goto cleanup;
        }
    }
    
    // Run all coroutines first time
    log->debug(log, "Running all coroutines first time");
    InfraxAsyncRun();
    
    // Verify first increment
    for (int i = 0; i < MAX_COROUTINES; i++) {
        if (states[i]->value != 1) {
            log->error(log, "First increment failed for coroutine %d", i);
            goto cleanup;
        }
    }
    
    // Resume all coroutines
    log->debug(log, "Resuming all coroutines");
    for (int i = 0; i < MAX_COROUTINES; i++) {
        InfraxError err = coroutines[i]->resume(coroutines[i]);
        if (!INFRAX_ERROR_IS_OK(err)) {
            log->error(log, "Failed to resume coroutine %d", i);
            goto cleanup;
        }
    }
    
    // Run all coroutines second time
    log->debug(log, "Running all coroutines second time");
    InfraxAsyncRun();
    
    // Verify second increment and completion
    for (int i = 0; i < MAX_COROUTINES; i++) {
        if (states[i]->value != 2) {
            log->error(log, "Second increment failed for coroutine %d", i);
            goto cleanup;
        }
        if (!coroutines[i]->is_done(coroutines[i])) {
            log->error(log, "Coroutine %d should be done", i);
            goto cleanup;
        }
    }
    
    log->debug(log, "Multiple coroutines test passed");

cleanup:
    for (int i = 0; i < MAX_COROUTINES; i++) {
        if (coroutines[i]) {
            InfraxAsync_CLASS.free(coroutines[i]);
        }
        if (states[i]) {
            free(states[i]);
        }
    }
}

void test_async_error_handling(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Testing error handling");
    
    // 分配测试状态
    TestState* state = malloc(sizeof(TestState));
    if (!state) {
        log->error(log, "Failed to allocate test state");
        return;
    }
    memset(state, 0, sizeof(TestState));
    
    // Test invalid config
    InfraxAsyncConfig config = {
        .name = "test_coroutine",
        .fn = NULL,  // Invalid function pointer
        .arg = state
    };
    
    InfraxAsync* co = InfraxAsync_CLASS.new(&config);
    if (co) {
        log->error(log, "Should not create coroutine with invalid config");
        InfraxAsync_CLASS.free(co);
        goto cleanup;
    }
    
    // Test valid config but invalid operations
    config.fn = test_coroutine_func;
    co = InfraxAsync_CLASS.new(&config);
    if (!co) {
        log->error(log, "Failed to create coroutine");
        goto cleanup;
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
    if (state) {
        free(state);
    }
}

int main(void) {
    test_async_basic();
    test_async_multiple();
    test_async_error_handling();
    return 0;
}
