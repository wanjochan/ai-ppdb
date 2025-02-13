#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

// Test timeout control
#define TEST_TIMEOUT_MS 2000  // 2 seconds timeout for each test
#define POLL_INTERVAL_MS 10   // Initial poll interval
#define MAX_POLL_INTERVAL_MS 100  // Maximum poll interval

// Test context structure
typedef struct {
    int counter;
    int target;
    bool has_error;
    char error_msg[256];
} TestContext;

// Helper function to get current timestamp in milliseconds
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// Test async function
static void test_async_fn(InfraxAsync* self, void* arg) {
    TestContext* ctx = (TestContext*)arg;
    if (!ctx) {
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    // Simulate potential errors
    if (ctx->has_error) {
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    // Update state to running
    self->state = INFRAX_ASYNC_RUNNING;
    
    // Increment counter
    ctx->counter++;
    
    // Check if we've reached the target
    if (ctx->counter >= ctx->target) {
        self->state = INFRAX_ASYNC_FULFILLED;
        return;
    }
    
    // Return to let other tasks run
    self->state = INFRAX_ASYNC_PENDING;
    return;
}

// Main test function
int main(void) {
    printf("Running InfraxAsync tests...\n");
    
    // Create test context
    TestContext ctx = {
        .counter = 0,
        .target = 5,
        .has_error = false,
        .error_msg = {0}
    };
    
    // Record start time for timeout control
    uint64_t start_time = get_current_time_ms();
    uint64_t current_time;
    int poll_interval = POLL_INTERVAL_MS;
    
    // Create async task
    InfraxAsync* async = InfraxAsyncClass.new(test_async_fn, &ctx);
    if (!async) {
        printf("Failed to create async task\n");
        return 1;
    }
    
    // Start task
    bool started = InfraxAsyncClass.start(async);
    if (!started) {
        printf("Failed to start async task\n");
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Poll until done or timeout
    while (!InfraxAsyncClass.is_done(async)) {
        // Check timeout
        current_time = get_current_time_ms();
        if (current_time - start_time >= TEST_TIMEOUT_MS) {
            printf("Test timeout after %d ms\n", TEST_TIMEOUT_MS);
            InfraxAsyncClass.cancel(async);
            InfraxAsyncClass.free(async);
            return 1;
        }
        
        // Poll with adaptive interval
        int ret = InfraxAsyncClass.pollset_poll(async, poll_interval);
        if (ret < 0) {
            printf("Poll failed with error: %d\n", ret);
            InfraxAsyncClass.free(async);
            return 1;
        }
        
        // Handle different states
        switch (async->state) {
            case INFRAX_ASYNC_PENDING:
                // Restart task
                if (!InfraxAsyncClass.start(async)) {
                    printf("Failed to restart async task\n");
                    InfraxAsyncClass.free(async);
                    return 1;
                }
                // Increase poll interval for efficiency
                poll_interval = poll_interval * 2;
                if (poll_interval > MAX_POLL_INTERVAL_MS) {
                    poll_interval = MAX_POLL_INTERVAL_MS;
                }
                break;
                
            case INFRAX_ASYNC_RUNNING:
                // Task is running, keep current poll interval
                break;
                
            case INFRAX_ASYNC_REJECTED:
                printf("Task was rejected\n");
                if (ctx.has_error) {
                    printf("Error: %s\n", ctx.error_msg);
                }
                InfraxAsyncClass.free(async);
                return 1;
                
            case INFRAX_ASYNC_FULFILLED:
                // Will be handled by the loop condition
                break;
        }
        
        // Reset poll interval if making progress
        if (ctx.counter > 0 && poll_interval > POLL_INTERVAL_MS) {
            poll_interval = POLL_INTERVAL_MS;
        }
    }
    
    // Check final state and results
    if (async->state != INFRAX_ASYNC_FULFILLED) {
        printf("Task did not complete successfully. Final state: %d\n", async->state);
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    if (ctx.counter != ctx.target) {
        printf("Counter mismatch: expected %d, got %d\n", ctx.target, ctx.counter);
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Calculate and print performance metrics
    current_time = get_current_time_ms();
    printf("Test completed in %lu ms\n", current_time - start_time);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    
    printf("All InfraxAsync tests passed!\n");
    return 0;
}
