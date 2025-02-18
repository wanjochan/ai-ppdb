#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
#include <signal.h>

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

// Timer test handlers
static void timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    char discard_buf[256];
    if (fd >= 0) {
        while (read(fd, discard_buf, sizeof(discard_buf)) > 0) {}  // 完全清空管道
    }
    core->printf(NULL,"Timer event received!\n");
    *(int*)arg = 1;  // Set result
}

static void multi_timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    char discard_buf[256];
    if (fd >= 0) {
        while (read(fd, discard_buf, sizeof(discard_buf)) > 0) {}  // 完全清空管道
    }
    int* timer_count = (int*)arg;
    (*timer_count)++;
    core->printf(NULL, "Timer %d fired!\n", *timer_count);
}

static void concurrent_timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    TestContext* ctx = (TestContext*)arg;
    char discard_buf[256];
    if (fd >= 0) {
        while (read(fd, discard_buf, sizeof(discard_buf)) > 0) {}  // 完全清空管道
    }
    
    ctx->counter++;
    uint64_t now = get_current_time_ms();
    
    if (ctx->counter % 10 == 0 || ctx->counter == ctx->target) {
        core->printf(NULL, "Progress: %d/%d timers fired (%.2f%%)\n", 
                    ctx->counter, ctx->target,
                    (float)ctx->counter * 100 / ctx->target);
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
    uint64_t start_time = get_current_time_ms();
    while (!result && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
        
        // 每秒打印一次进度
        static uint64_t last_progress = 0;
        uint64_t now = get_current_time_ms();
        if (now - last_progress >= 1000) {
            core->printf(NULL, "Waiting for timer... (elapsed: %lu ms)\n", 
                        now - start_time);
            last_progress = now;
        }
        
        // 检查是否超时
        if (now - start_time > 2000) {  // 2秒超时
            core->printf(NULL, "Timer did not expire in time\n");
            InfraxAsyncClass.clearTimeout(timer_id);
            InfraxAsyncClass.free(async);
            clear_timeout();
            return;
        }
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
    
    uint64_t end_time = get_current_time_ms();
    uint64_t elapsed = end_time - start_time;
    core->printf(NULL, "Timer test passed (elapsed: %lu ms)\n", elapsed);
    
    InfraxAsyncClass.clearTimeout(timer_id);
    InfraxAsyncClass.free(async);
    clear_timeout();
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
    uint64_t start_time = get_current_time_ms();
    while (timer_count < 2 && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
        
        // 每秒打印一次进度
        static uint64_t last_progress = 0;
        uint64_t now = get_current_time_ms();
        if (now - last_progress >= 1000) {
            core->printf(NULL, "Waiting for timers... (elapsed: %lu ms, count: %d/2)\n", 
                        now - start_time, timer_count);
            last_progress = now;
        }
        
        // 检查是否超时
        if (now - start_time > 3000) {  // 3秒超时
            core->printf(NULL, "Not all timers fired in time\n");
            InfraxAsyncClass.clearTimeout(timer1);
            InfraxAsyncClass.clearTimeout(timer2);
            InfraxAsyncClass.free(async);
            clear_timeout();
            return;
        }
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
    
    uint64_t end_time = get_current_time_ms();
    uint64_t elapsed = end_time - start_time;
    core->printf(NULL, "Multiple timers test passed (elapsed: %lu ms)\n", elapsed);
    
    InfraxAsyncClass.clearTimeout(timer1);
    InfraxAsyncClass.clearTimeout(timer2);
    InfraxAsyncClass.free(async);
    clear_timeout();
}

// 并发定时器测试
#define CONCURRENT_TIMER_COUNT 100

void test_concurrent_timers() {
    core->printf(NULL, "Testing %d concurrent timers...\n", CONCURRENT_TIMER_COUNT);
    setup_timeout(30);  // 增加超时时间到30秒
    
    // Create async task for polling
    InfraxAsync* async = InfraxAsyncClass.new(NULL, NULL);
    if (!async) {
        core->printf(NULL,"Failed to create async task\n");
        clear_timeout();
        return;
    }
    
    // 创建测试上下文
    TestContext ctx = {
        .counter = 0,
        .target = CONCURRENT_TIMER_COUNT,
        .has_error = false
    };
    
    // 记录开始时间
    uint64_t start_time = get_current_time_ms();
    
    // 创建定时器数组
    InfraxU32 timer_ids[CONCURRENT_TIMER_COUNT];
    
    // 设置定时器，间隔从100ms到1000ms不等
    core->printf(NULL, "Creating %d timers...\n", CONCURRENT_TIMER_COUNT);
    for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
        InfraxU32 interval = 100 + (i % 10) * 100;  // 100ms到1000ms
        timer_ids[i] = InfraxAsyncClass.setTimeout(interval, concurrent_timer_handler, &ctx);
        if (timer_ids[i] == 0) {
            core->printf(NULL, "Failed to set timer %d\n", i);
            ctx.has_error = true;
            snprintf(ctx.error_msg, sizeof(ctx.error_msg), "Failed to create timer %d", i);
            // 清理已创建的定时器
            for (int j = 0; j < i; j++) {
                InfraxAsyncClass.clearTimeout(timer_ids[j]);
            }
            InfraxAsyncClass.free(async);
            clear_timeout();
            return;
        }
    }
    core->printf(NULL, "All timers created successfully\n");
    
    // 等待所有定时器触发
    core->printf(NULL, "Waiting for timers to fire...\n");
    while (ctx.counter < CONCURRENT_TIMER_COUNT && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
        
        // 每秒打印一次进度
        static uint64_t last_progress = 0;
        uint64_t now = get_current_time_ms();
        if (now - last_progress >= 1000) {
            core->printf(NULL, "Progress: %d/%d timers fired (%.2f%%)\n", 
                        ctx.counter, ctx.target,
                        (float)ctx.counter * 100 / ctx.target);
            last_progress = now;
        }
    }
    
    // 记录结束时间
    uint64_t end_time = get_current_time_ms();
    uint64_t total_time = end_time - start_time;
    
    if (test_timeout) {
        core->printf(NULL,"Test timed out after %lu ms. Only %d/%d timers fired.\n", 
                    total_time, ctx.counter, CONCURRENT_TIMER_COUNT);
        // 清理定时器
        for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
            InfraxAsyncClass.clearTimeout(timer_ids[i]);
        }
        InfraxAsyncClass.free(async);
        clear_timeout();
        return;
    }
    
    if (ctx.counter != CONCURRENT_TIMER_COUNT) {
        core->printf(NULL, "Not all timers fired (count=%d/%d) after %lu ms\n", 
                    ctx.counter, CONCURRENT_TIMER_COUNT, total_time);
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
    
    // 打印性能统计
    core->printf(NULL, "\nPerformance Statistics:\n");
    core->printf(NULL, "Total time: %lu ms\n", total_time);
    core->printf(NULL, "Average time per timer: %.2f ms\n", 
                (float)total_time / CONCURRENT_TIMER_COUNT);
    core->printf(NULL, "Timers per second: %.2f\n", 
                (float)CONCURRENT_TIMER_COUNT * 1000 / total_time);
    
    core->printf(NULL, "Concurrent timers test passed\n");
}

int main(void) {
    core = InfraxCoreClass.singleton();
    if (!core) {
        printf("Failed to get core singleton\n");
        return 1;
    }
    
    test_async_timer();
    test_multiple_timers();
    test_concurrent_timers();
    
    return 0;
}
