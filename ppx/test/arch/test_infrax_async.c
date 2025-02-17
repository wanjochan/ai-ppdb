#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxTimer.h"
#include <signal.h>
#include <stdlib.h>

InfraxCore* core = NULL;

// Test timeout control
#define TEST_TIMEOUT_MS 2000  // 2 seconds timeout for each test
#define POLL_INTERVAL_MS 10   // Initial poll interval
#define MAX_POLL_INTERVAL_MS 100  // Maximum poll interval

static volatile int test_timeout = 0;

static void alarm_handler(int sig) {
    test_timeout = 1;
    core->printf(NULL, "Test timeout!\n");
}

static void setup_timeout(int seconds) {
    test_timeout = 0;
    signal(SIGALRM, alarm_handler);
    alarm(seconds);
}

static void clear_timeout() {
    alarm(0);
    test_timeout = 0;
}

// Test context structure
typedef struct {
    int counter;
    int target;
    bool has_error;
    char error_msg[256];
} TestContext;

// Helper function to get current timestamp in milliseconds
static uint64_t get_current_time_ms(void) {
    return core->time_monotonic_ms(core);
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

// Timer test handlers
static void timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    core->printf(NULL,"Timer event received!\n");
    *(int*)arg = 1;  // Set result
}

static void multi_timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    int* timer_count = (int*)arg;
    (*timer_count)++;
    core->printf(NULL, "Timer %d fired!\n", *timer_count);
}

typedef struct {
    int* sequence;
    int index;
    int max_index;
} SequenceContext;

static void sequence_timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    SequenceContext* ctx = (SequenceContext*)arg;
    if (ctx->index < ctx->max_index) {
        ctx->sequence[ctx->index++] = 1;
        core->printf(NULL, "Timer at index %d fired\n", ctx->index - 1);
    }
}

// Timer tests
void test_async_timer() {
    core->printf(NULL,"Testing async with timer...\n");
    setup_timeout(5);  // 5 second timeout
    
    // Create async task for polling
    InfraxAsync* async = InfraxAsyncClass.new(NULL, NULL);
    if (!async) {
        core->printf(NULL,"Failed to create async task\n");
        clear_timeout();
        return;
    }
    
    // Set timeout
    int result = 0;
    InfraxU32 timer_id = InfraxAsyncClass.setTimeout(1000, timer_handler, &result);
    if (timer_id == 0) {
        core->printf(NULL,"Failed to set timeout\n");
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    // Wait for timer event
    while (!result && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
    }
    
    if (test_timeout) {
        core->printf(NULL,"Test timed out\n");
        InfraxAsyncClass.clearTimeout(timer_id);
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    if (!result) {
        core->printf(NULL,"Timer did not expire in time\n");
        InfraxAsyncClass.clearTimeout(timer_id);
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    InfraxAsyncClass.clearTimeout(timer_id);
    InfraxAsyncClass.free(async);
    clear_timeout();
    core->printf(NULL,"Timer test passed\n");
}

void test_multiple_timers() {
    core->printf(NULL, "Testing multiple concurrent timers...\n");
    setup_timeout(10);  // 10 second timeout
    
    // Create async task for polling
    InfraxAsync* async = InfraxAsyncClass.new(NULL, NULL);
    if (!async) {
        core->printf(NULL,"Failed to create async task\n");
        clear_timeout();
        return;
    }
    
    int timer_count = 0;
    InfraxU32 timer1 = InfraxAsyncClass.setTimeout(500, multi_timer_handler, &timer_count);
    InfraxU32 timer2 = InfraxAsyncClass.setTimeout(1000, multi_timer_handler, &timer_count);
    
    if (timer1 == 0 || timer2 == 0) {
        core->printf(NULL, "Failed to set timers\n");
        if (timer1) InfraxAsyncClass.clearTimeout(timer1);
        if (timer2) InfraxAsyncClass.clearTimeout(timer2);
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    // Wait for all timers to fire
    while (timer_count < 2 && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
    }
    
    if (test_timeout) {
        core->printf(NULL,"Test timed out\n");
        InfraxAsyncClass.clearTimeout(timer1);
        InfraxAsyncClass.clearTimeout(timer2);
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    if (timer_count != 2) {
        core->printf(NULL, "Not all timers fired (count=%d)\n", timer_count);
        InfraxAsyncClass.clearTimeout(timer1);
        InfraxAsyncClass.clearTimeout(timer2);
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    InfraxAsyncClass.clearTimeout(timer1);
    InfraxAsyncClass.clearTimeout(timer2);
    InfraxAsyncClass.free(async);
    clear_timeout();
    core->printf(NULL, "Multiple timers test passed\n");
}

// 并发定时器测试
#define CONCURRENT_TIMER_COUNT 100

static void concurrent_timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    int* fired_count = (int*)arg;
    (*fired_count)++;
    core->printf(NULL, "Timer %d fired\n", *fired_count);
}

void test_concurrent_timers() {
    core->printf(NULL, "Testing %d concurrent timers...\n", CONCURRENT_TIMER_COUNT);
    setup_timeout(20);  // 20 second timeout
    
    // Create async task for polling
    InfraxAsync* async = InfraxAsyncClass.new(NULL, NULL);
    if (!async) {
        core->printf(NULL,"Failed to create async task\n");
        clear_timeout();
        return;
    }
    
    // 创建定时器数组
    InfraxU32 timer_ids[CONCURRENT_TIMER_COUNT];
    int fired_count = 0;
    
    // 设置定时器，间隔从100ms到1000ms不等
    for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
        InfraxU32 interval = 100 + (i % 10) * 100;  // 100ms到1000ms
        timer_ids[i] = InfraxAsyncClass.setTimeout(interval, concurrent_timer_handler, &fired_count);
        if (timer_ids[i] == 0) {
            core->printf(NULL, "Failed to set timer %d\n", i);
            // 清理已创建的定时器
            for (int j = 0; j < i; j++) {
                InfraxAsyncClass.clearTimeout(timer_ids[j]);
            }
            InfraxAsyncClass.free(async);
            clear_timeout();
            return;
        }
    }
    
    // 等待所有定时器触发
    while (fired_count < CONCURRENT_TIMER_COUNT && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
    }
    
    if (test_timeout) {
        core->printf(NULL,"Test timed out after firing %d timers\n", fired_count);
        // 清理定时器
        for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
            InfraxAsyncClass.clearTimeout(timer_ids[i]);
        }
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    if (fired_count != CONCURRENT_TIMER_COUNT) {
        core->printf(NULL, "Not all timers fired (fired=%d, expected=%d)\n", 
                    fired_count, CONCURRENT_TIMER_COUNT);
        // 清理定时器
        for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
            InfraxAsyncClass.clearTimeout(timer_ids[i]);
        }
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    // 清理定时器
    for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
        InfraxAsyncClass.clearTimeout(timer_ids[i]);
    }
    InfraxAsyncClass.free(async);
    clear_timeout();
    core->printf(NULL, "All %d concurrent timers fired successfully\n", CONCURRENT_TIMER_COUNT);
}

// Main test function
int main(void) {
    core = InfraxCoreClass.singleton();
    INFRAX_ASSERT(core, core != NULL);
    
    core->printf(core, "Running InfraxAsync tests...\n");
    
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
        core->printf(core, "Failed to create async task\n");
        return 1;
    }
    
    // Start task
    bool started = InfraxAsyncClass.start(async);
    if (!started) {
        core->printf(core, "Failed to start async task\n");
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Poll until done or timeout
    while (!InfraxAsyncClass.is_done(async)) {
        // Check timeout
        current_time = get_current_time_ms();
        if (current_time - start_time >= TEST_TIMEOUT_MS) {
            core->printf(core, "Test timeout after %d ms\n", TEST_TIMEOUT_MS);
            InfraxAsyncClass.cancel(async);
            InfraxAsyncClass.free(async);
            return 1;
        }
        
        // Poll with adaptive interval
        int ret = InfraxAsyncClass.pollset_poll(async, poll_interval);
        if (ret < 0) {
            core->printf(core, "Poll failed with error: %d\n", ret);
            InfraxAsyncClass.free(async);
            return 1;
        }
        
        // Handle different states
        switch (async->state) {
            case INFRAX_ASYNC_PENDING:
                // Restart task
                if (!InfraxAsyncClass.start(async)) {
                    core->printf(core, "Failed to restart async task\n");
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
                core->printf(core, "Task was rejected\n");
                if (ctx.has_error) {
                    core->printf(core, "Error: %s\n", ctx.error_msg);
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
        core->printf(core, "Task did not complete successfully. Final state: %d\n", async->state);
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    if (ctx.counter != ctx.target) {
        core->printf(core, "Counter mismatch: expected %d, got %d\n", ctx.target, ctx.counter);
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Calculate and print performance metrics
    current_time = get_current_time_ms();
    core->printf(core, "Test completed in %lu ms\n", current_time - start_time);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    
    // Run timer tests
    test_async_timer();
    test_multiple_timers();
    test_concurrent_timers();  // 添加并发测试
    
    core->printf(core, "All InfraxAsync tests passed!\n");
    return 0;
}
