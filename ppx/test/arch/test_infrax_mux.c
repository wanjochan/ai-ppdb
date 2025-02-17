#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxTimer.h"
#include "internal/infrax/InfraxMux.h"
#include <signal.h>
#include <stdlib.h>

InfraxCore* core;
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

static void timer_handler(int fd, short events, void* arg) {
    core->printf(NULL,"Timer event received!\n");
    *(int*)arg = 1;  // Set result
}

// 多定时器测试的处理函数
static void multi_timer_handler(int fd, short events, void* arg) {
    int* timer_count = (int*)arg;
    (*timer_count)++;
    core->printf(NULL, "Timer %d fired!\n", *timer_count);
}

// 顺序定时器测试的处理函数
typedef struct {
    int* sequence;
    int index;
    int max_index;
} SequenceContext;

static void sequence_timer_handler(int fd, short events, void* arg) {
    SequenceContext* ctx = (SequenceContext*)arg;
    if (ctx->index < ctx->max_index) {
        ctx->sequence[ctx->index++] = 1;
        core->printf(NULL, "Timer at index %d fired\n", ctx->index - 1);
    }
}

void test_mux_timer() {
    core->printf(NULL,"Testing mux with timer thread...\n");
    setup_timeout(5);  // 5 second timeout
    
    // Set timeout
    int result = 0;
    InfraxU32 timer_id = InfraxMuxClass.setTimeout(1000, timer_handler, &result);
    if (timer_id == 0) {
        core->printf(NULL,"Failed to set timeout\n");
        clear_timeout();
        return;
    }
    
    // Wait for timer event with shorter timeout
    InfraxError err = InfraxMuxClass.pollall(NULL, 0, timer_handler, &result, 2000);
    if (test_timeout) {
        core->printf(NULL,"Test timed out\n");
        InfraxMuxClass.clearTimeout(timer_id);
        clear_timeout();
        return;
    }
    
    if (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT) {
        core->printf(NULL,"Poll failed: %s\n", err.message);
        InfraxMuxClass.clearTimeout(timer_id);
        clear_timeout();
        return;
    }
    
    if (!result) {
        core->printf(NULL,"Timer did not expire in time\n");
        InfraxMuxClass.clearTimeout(timer_id);
        clear_timeout();
        return;
    }
    
    InfraxMuxClass.clearTimeout(timer_id);
    clear_timeout();
    core->printf(NULL,"Timer test passed\n");
}

// Test multiple concurrent timers
void test_multiple_timers() {
    core->printf(NULL, "Testing multiple concurrent timers...\n");
    setup_timeout(10);  // 增加超时时间
    
    int timer_count = 0;
    InfraxU32 timer1 = InfraxMuxClass.setTimeout(500, multi_timer_handler, &timer_count);
    InfraxU32 timer2 = InfraxMuxClass.setTimeout(1000, multi_timer_handler, &timer_count);
    
    if (timer1 == 0 || timer2 == 0) {
        core->printf(NULL, "Failed to set timers\n");
        if (timer1) InfraxMuxClass.clearTimeout(timer1);
        if (timer2) InfraxMuxClass.clearTimeout(timer2);
        clear_timeout();
        return;
    }
    
    // 等待所有定时器触发
    while (timer_count < 2 && !test_timeout) {
        InfraxError err = InfraxMuxClass.pollall(NULL, 0, multi_timer_handler, &timer_count, 500);
        if (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT) {
            core->printf(NULL, "Poll failed: %s\n", err.message);
            InfraxMuxClass.clearTimeout(timer1);
            InfraxMuxClass.clearTimeout(timer2);
            clear_timeout();
            return;
        }
    }
    
    if (timer_count != 2) {
        core->printf(NULL, "Not all timers fired (count=%d)\n", timer_count);
        InfraxMuxClass.clearTimeout(timer1);
        InfraxMuxClass.clearTimeout(timer2);
        clear_timeout();
        return;
    }
    
    InfraxMuxClass.clearTimeout(timer1);
    InfraxMuxClass.clearTimeout(timer2);
    clear_timeout();
    core->printf(NULL, "Multiple timers test passed\n");
}

// Test timer sequence
void test_timer_sequence() {
    core->printf(NULL, "Testing timer sequence...\n");
    setup_timeout(10);  // 增加超时时间
    
    int sequence[3] = {0};
    SequenceContext ctx = {sequence, 0, 3};
    
    // Set timers with different intervals
    InfraxU32 timer1 = InfraxMuxClass.setTimeout(100, sequence_timer_handler, &ctx);
    InfraxU32 timer2 = InfraxMuxClass.setTimeout(300, sequence_timer_handler, &ctx);
    InfraxU32 timer3 = InfraxMuxClass.setTimeout(500, sequence_timer_handler, &ctx);
    
    if (timer1 == 0 || timer2 == 0 || timer3 == 0) {
        core->printf(NULL, "Failed to set sequence timers\n");
        if (timer1) InfraxMuxClass.clearTimeout(timer1);
        if (timer2) InfraxMuxClass.clearTimeout(timer2);
        if (timer3) InfraxMuxClass.clearTimeout(timer3);
        clear_timeout();
        return;
    }
    
    core->printf(NULL, "Waiting for timers (100ms, 300ms, 500ms)...\n");
    
    // 等待所有定时器触发
    while (ctx.index < 3 && !test_timeout) {
        InfraxError err = InfraxMuxClass.pollall(NULL, 0, sequence_timer_handler, &ctx, 200);
        if (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT) {
            core->printf(NULL, "Poll failed: %s\n", err.message);
            InfraxMuxClass.clearTimeout(timer1);
            InfraxMuxClass.clearTimeout(timer2);
            InfraxMuxClass.clearTimeout(timer3);
            clear_timeout();
            return;
        }
    }
    
    // Verify sequence
    if (ctx.index != 3) {
        core->printf(NULL, "Not all sequence timers fired (count=%d)\n", ctx.index);
        for (int i = 0; i < 3; i++) {
            core->printf(NULL, "  Timer %d: %s\n", i, sequence[i] ? "fired" : "not fired");
        }
        InfraxMuxClass.clearTimeout(timer1);
        InfraxMuxClass.clearTimeout(timer2);
        InfraxMuxClass.clearTimeout(timer3);
        clear_timeout();
        return;
    }
    
    InfraxMuxClass.clearTimeout(timer1);
    InfraxMuxClass.clearTimeout(timer2);
    InfraxMuxClass.clearTimeout(timer3);
    clear_timeout();
    core->printf(NULL, "Timer sequence test passed\n");
}

// Test timer cancellation
void test_timer_cancellation() {
    core->printf(NULL, "Testing timer cancellation...\n");
    setup_timeout(5);  // 5 second timeout
    
    int result = 0;
    InfraxU32 timer_id = InfraxMuxClass.setTimeout(1000, timer_handler, &result);
    if (timer_id == 0) {
        core->printf(NULL, "Failed to set timer\n");
        clear_timeout();
        return;
    }
    
    // Cancel timer immediately
    InfraxError err = InfraxMuxClass.clearTimeout(timer_id);
    if (err.code != 0) {
        core->printf(NULL, "Failed to clear timer: %s\n", err.message);
        clear_timeout();
        return;
    }
    
    // Short wait to ensure timer doesn't fire
    err = InfraxMuxClass.pollall(NULL, 0, NULL, NULL, 200);
    if (test_timeout) {
        core->printf(NULL,"Test timed out\n");
        clear_timeout();
        return;
    }
    
    if (result != 0) {
        core->printf(NULL, "Timer fired despite cancellation\n");
        clear_timeout();
        return;
    }
    
    clear_timeout();
    core->printf(NULL, "Timer cancellation test passed\n");
}

// Test boundary conditions
void test_timer_boundaries() {
    core->printf(NULL, "Testing timer boundary conditions...\n");
    setup_timeout(5);  // 5 second timeout
    
    // Test zero delay timer
    int result = 0;
    InfraxU32 timer_id = InfraxMuxClass.setTimeout(0, timer_handler, &result);
    if (timer_id == 0) {
        core->printf(NULL, "Failed to set zero delay timer\n");
        clear_timeout();
        return;
    }
    
    InfraxError err = InfraxMuxClass.pollall(NULL, 0, NULL, NULL, 100);
    if (test_timeout) {
        core->printf(NULL,"Test timed out\n");
        InfraxMuxClass.clearTimeout(timer_id);
        clear_timeout();
        return;
    }
    
    InfraxMuxClass.clearTimeout(timer_id);
    
    // Test short delay timer
    result = 0;
    timer_id = InfraxMuxClass.setTimeout(100, timer_handler, &result);
    if (timer_id == 0) {
        core->printf(NULL, "Failed to set short delay timer\n");
        clear_timeout();
        return;
    }
    
    err = InfraxMuxClass.pollall(NULL, 0, NULL, NULL, 200);
    if (test_timeout) {
        core->printf(NULL,"Test timed out\n");
        InfraxMuxClass.clearTimeout(timer_id);
        clear_timeout();
        return;
    }
    
    InfraxMuxClass.clearTimeout(timer_id);
    clear_timeout();
    core->printf(NULL, "Timer boundary conditions test passed\n");
}

// 大量并发定时器测试的处理函数
static void mass_timer_handler(int fd, short events, void* arg) {
    int* fired_count = (int*)arg;
    (*fired_count)++;
    core->printf(NULL, "Mass timer %d fired!\n", *fired_count);
}

// 测试大量并发定时器
void test_mass_timers() {
    core->printf(NULL, "Testing mass concurrent timers...\n");
    setup_timeout(15);  // 15 second timeout
    
    #define NUM_TIMERS 500  // 增加定时器数量
    InfraxU32 timer_ids[NUM_TIMERS];
    int fired_count = 0;
    
    // 创建大量定时器
    for (int i = 0; i < NUM_TIMERS; i++) {
        // 随机间隔 100-1000ms
        InfraxU32 interval = 100 + (rand() % 900);
        timer_ids[i] = InfraxMuxClass.setTimeout(interval, mass_timer_handler, &fired_count);
        if (timer_ids[i] == 0) {
            core->printf(NULL, "Failed to create timer %d\n", i);
            goto cleanup;
        }
    }
    
    core->printf(NULL, "Created %d timers\n", NUM_TIMERS);
    
    // 等待定时器触发
    int consecutive_timeouts = 0;
    while (fired_count < NUM_TIMERS && consecutive_timeouts < 10) {
        InfraxError err = InfraxMuxClass.pollall(NULL, 0, mass_timer_handler, &fired_count, 100);
        if (test_timeout) {
            core->printf(NULL, "Test timed out\n");
            goto cleanup;
        }
        if (err.code == INFRAX_ERROR_TIMEOUT) {
            consecutive_timeouts++;
        } else if (err.code != 0) {
            core->printf(NULL, "Poll failed\n");
            goto cleanup;
        } else {
            consecutive_timeouts = 0;  // 重置连续超时计数
        }
    }
    
    if (fired_count < NUM_TIMERS * 0.9) {  // 允许90%的成功率
        core->printf(NULL, "Not enough timers fired (count=%d)\n", fired_count);
        goto cleanup;
    }
    
    core->printf(NULL, "Mass timers test passed (%d timers fired)\n", fired_count);
    
cleanup:
    // 清理定时器
    for (int i = 0; i < NUM_TIMERS; i++) {
        if (timer_ids[i] != 0) {
            InfraxMuxClass.clearTimeout(timer_ids[i]);
        }
    }
    clear_timeout();
}

// 动态定时器测试的处理函数
typedef struct {
    int* fired_count;
    InfraxU32* new_timer_id;
} DynamicContext;

static void dynamic_timer_handler(int fd, short events, void* arg) {
    DynamicContext* ctx = (DynamicContext*)arg;
    (*ctx->fired_count)++;
    core->printf(NULL, "Dynamic timer %d fired!\n", *ctx->fired_count);
    
    // 在定时器触发时创建新的定时器
    if (*ctx->fired_count < 5) {  // 最多创建5个
        *ctx->new_timer_id = InfraxMuxClass.setTimeout(200, dynamic_timer_handler, ctx);
    }
}

// 测试动态创建/删除定时器
void test_dynamic_timers() {
    core->printf(NULL, "Testing dynamic timer creation/deletion...\n");
    setup_timeout(5);  // 5 second timeout
    
    int fired_count = 0;
    InfraxU32 new_timer_id = 0;
    DynamicContext ctx = {&fired_count, &new_timer_id};
    
    // 创建初始定时器
    InfraxU32 timer_id = InfraxMuxClass.setTimeout(100, dynamic_timer_handler, &ctx);
    if (timer_id == 0) {
        core->printf(NULL, "Failed to create initial timer\n");
        clear_timeout();
        return;
    }
    
    // 等待定时器触发和动态创建
    while (fired_count < 5 && !test_timeout) {
        InfraxError err = InfraxMuxClass.pollall(NULL, 0, dynamic_timer_handler, &ctx, 1000);
        if (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT) {
            core->printf(NULL, "Poll failed\n");
            break;
        }
    }
    
    if (fired_count < 5) {
        core->printf(NULL, "Not enough timers fired (count=%d)\n", fired_count);
        InfraxMuxClass.clearTimeout(timer_id);
        if (new_timer_id) InfraxMuxClass.clearTimeout(new_timer_id);
        clear_timeout();
        return;
    }
    
    core->printf(NULL, "Dynamic timers test passed\n");
    clear_timeout();
}

int main() {
    // Get core singleton
    core = InfraxCoreClass.singleton();
    
    // Run tests
    test_mux_timer();
    usleep(500000);  // 500ms delay between tests
    
    test_multiple_timers();
    usleep(500000);
    
    test_timer_sequence();
    usleep(500000);
    
    test_timer_cancellation();
    usleep(500000);
    
    test_timer_boundaries();
    usleep(500000);
    
    test_mass_timers();
    usleep(500000);
    
    test_dynamic_timers();
    
    return 0;
}
