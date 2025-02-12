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

// Test timeout control
#define TEST_TIMEOUT_MS 2000  // 2 seconds timeout for each test

// Test context structure
typedef struct {
    int counter;
    int target;
} TestContext;

// Test async function
static void test_async_fn(InfraxAsync* self, void* arg) {
    TestContext* ctx = (TestContext*)arg;
    if (!ctx) return;
    
    // Increment counter
    ctx->counter++;
    
    // Check if we've reached the target
    if (ctx->counter >= ctx->target) {
        self->state = INFRAX_ASYNC_FULFILLED;
        return;
    }
    
    // Return to let other tasks run
    return;
}

// Main test function
int main(void) {
    printf("Running InfraxAsync tests...\n");
    
    // Create test context
    TestContext ctx = {
        .counter = 0,
        .target = 5
    };
    
    // Create async task
    InfraxAsync* async = InfraxAsyncClass.new(test_async_fn, &ctx);
    assert(async != NULL);
    
    // Start task
    bool started = InfraxAsyncClass.start(async);
    assert(started);
    
    // Poll until done
    while (!InfraxAsyncClass.is_done(async)) {
        int ret = InfraxAsyncClass.pollset_poll(async, 100);  // 100ms timeout
        assert(ret >= 0);
        
        // Restart task if pending
        if (async->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(async);
        }
    }
    
    // Check result
    assert(async->state == INFRAX_ASYNC_FULFILLED);
    assert(ctx.counter == ctx.target);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    
    printf("All InfraxAsync tests passed!\n");
    return 0;
}
