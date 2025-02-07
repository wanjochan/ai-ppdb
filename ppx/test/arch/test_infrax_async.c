#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxLog.h"
#include <string.h>
#include <assert.h>

// Test state
typedef struct {
    int value;
    InfraxAsync* co;
} TestState;

// Test coroutine function with timer event
static void test_timer_coroutine(void* arg) {
    TestState* state = (TestState*)arg;
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Timer coroutine started");
    
    InfraxScheduler* scheduler = get_default_scheduler();
    InfraxEventSource* timer = scheduler->create_timer_event(scheduler, 100);  // 100ms timeout
    
    // First increment
    state->value++;
    log->debug(log, "First increment done, value = %d", state->value);
    
    // Wait for timer
    int wait_result = state->co->wait(state->co, timer);
    if (wait_result < 0) {
        log->error(log, "Timer wait failed");
        timer->klass->free(timer);
        return;
    }
    
    // Run scheduler to process timer event
    scheduler->run(scheduler);
    
    // Second increment after timer
    state->value++;
    log->debug(log, "Second increment done, value = %d", state->value);
    log->debug(log, "Timer coroutine finished");
    
    timer->klass->free(timer);
}

// Test coroutine function with custom event
static int custom_ready(void* source) {
    return *(int*)source > 0;
}

static int custom_wait(void* source) {
    return 0;  // Non-blocking
}

static void custom_cleanup(void* source) {
    free(source);
}

static void test_custom_coroutine(void* arg) {
    TestState* state = (TestState*)arg;
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Custom coroutine started");
    
    InfraxScheduler* scheduler = get_default_scheduler();
    int* counter = malloc(sizeof(int));
    *counter = 0;
    
    InfraxEventSource* event = scheduler->create_custom_event(
        scheduler,
        counter,
        custom_ready,
        custom_wait,
        custom_cleanup
    );
    
    // First increment
    state->value++;
    log->debug(log, "First increment done, value = %d", state->value);
    
    // Wait for custom event
    *counter = 1;  // Make event ready
    int wait_result = state->co->wait(state->co, event);
    if (wait_result < 0) {
        log->error(log, "Custom event wait failed");
        event->klass->free(event);
        return;
    }
    
    // Run scheduler to process custom event
    scheduler->run(scheduler);
    
    // Second increment after event
    state->value++;
    log->debug(log, "Second increment done, value = %d", state->value);
    log->debug(log, "Custom coroutine finished");
    
    event->klass->free(event);
}

// Basic timer test
void test_async_timer(void) {
    TestState state = {0};
    InfraxAsyncConfig config = {
        .fn = test_timer_coroutine,
        .arg = &state
    };
    
    InfraxAsync* co = InfraxAsync_new(&config);
    assert(co != NULL);
    state.co = co;
    
    InfraxScheduler* scheduler = get_default_scheduler();
    scheduler->run(scheduler);
    
    assert(state.value == 2);
    assert(co->is_done(co));
    
    co->klass->free(co);
}

// Custom event test
void test_async_custom(void) {
    TestState state = {0};
    InfraxAsyncConfig config = {
        .fn = test_custom_coroutine,
        .arg = &state
    };
    
    InfraxAsync* co = InfraxAsync_new(&config);
    assert(co != NULL);
    state.co = co;
    
    InfraxScheduler* scheduler = get_default_scheduler();
    scheduler->run(scheduler);
    
    assert(state.value == 2);
    assert(co->is_done(co));
    
    co->klass->free(co);
}

// Multiple coroutines test
void test_async_multiple(void) {
    // Timer coroutine state
    TestState timer_state = {0};
    InfraxAsyncConfig timer_config = {
        .fn = test_timer_coroutine,
        .arg = &timer_state
    };
    
    // Custom coroutine state
    TestState custom_state = {0};
    InfraxAsyncConfig custom_config = {
        .fn = test_custom_coroutine,
        .arg = &custom_state
    };
    
    // Create coroutines
    InfraxAsync* timer_co = InfraxAsync_new(&timer_config);
    assert(timer_co != NULL);
    timer_state.co = timer_co;
    
    InfraxAsync* custom_co = InfraxAsync_new(&custom_config);
    assert(custom_co != NULL);
    custom_state.co = custom_co;
    
    // Run both coroutines
    InfraxScheduler* scheduler = get_default_scheduler();
    scheduler->run(scheduler);
    
    // Check results
    assert(timer_state.value == 2);
    assert(custom_state.value == 2);
    assert(timer_co->is_done(timer_co));
    assert(custom_co->is_done(custom_co));
    
    // Cleanup
    timer_co->klass->free(timer_co);
    custom_co->klass->free(custom_co);
}

// Error handling test
void test_async_error_handling(void) {
    TestState state = {0};
    InfraxAsyncConfig config = {
        .fn = test_timer_coroutine,
        .arg = &state
    };
    
    InfraxAsync* co = InfraxAsync_new(&config);
    assert(co != NULL);
    state.co = co;
    
    InfraxScheduler* scheduler = get_default_scheduler();
    InfraxEventSource* event = scheduler->create_custom_event(
        scheduler,
        NULL,  // Invalid source
        NULL,  // Invalid ready function
        NULL,  // Invalid wait function
        NULL   // Invalid cleanup function
    );
    
    assert(event == NULL);  // Should fail to create event
    
    // Cleanup
    co->klass->free(co);
}

int main(void) {
    InfraxLog* log = get_global_infrax_log();
    log->set_level(log, LOG_LEVEL_DEBUG);  // Set log level to DEBUG
    test_async_timer();
    test_async_custom();
    test_async_multiple();
    test_async_error_handling();
    return 0;
}
