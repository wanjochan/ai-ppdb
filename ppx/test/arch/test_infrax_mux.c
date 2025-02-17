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
    InfraxError err = InfraxMuxClass.pollall(NULL, 0, timer_handler, &result, 1500);
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
    setup_timeout(5);  // 5 second timeout
    
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
    
    // 等待第一个定时器
    InfraxError err = InfraxMuxClass.pollall(NULL, 0, multi_timer_handler, &timer_count, 750);
    if (test_timeout || (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT)) {
        core->printf(NULL, "First wait failed\n");
        InfraxMuxClass.clearTimeout(timer1);
        InfraxMuxClass.clearTimeout(timer2);
        clear_timeout();
        return;
    }
    
    // 等待第二个定时器
    err = InfraxMuxClass.pollall(NULL, 0, multi_timer_handler, &timer_count, 750);
    if (test_timeout || (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT)) {
        core->printf(NULL, "Second wait failed\n");
        InfraxMuxClass.clearTimeout(timer1);
        InfraxMuxClass.clearTimeout(timer2);
        clear_timeout();
        return;
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
    setup_timeout(5);  // 5 second timeout
    
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
    
    // 分段等待所有定时器
    InfraxError err;
    
    // 等待第一个定时器
    core->printf(NULL, "Waiting for first timer (100ms)...\n");
    err = InfraxMuxClass.pollall(NULL, 0, sequence_timer_handler, &ctx, 200);
    if (test_timeout || (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT)) {
        core->printf(NULL, "First sequence wait failed: %s\n", err.message);
        InfraxMuxClass.clearTimeout(timer1);
        InfraxMuxClass.clearTimeout(timer2);
        InfraxMuxClass.clearTimeout(timer3);
        clear_timeout();
        return;
    }
    
    // 等待第二个定时器
    core->printf(NULL, "Waiting for second timer (300ms)...\n");
    err = InfraxMuxClass.pollall(NULL, 0, sequence_timer_handler, &ctx, 300);
    if (test_timeout || (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT)) {
        core->printf(NULL, "Second sequence wait failed: %s\n", err.message);
        InfraxMuxClass.clearTimeout(timer1);
        InfraxMuxClass.clearTimeout(timer2);
        InfraxMuxClass.clearTimeout(timer3);
        clear_timeout();
        return;
    }
    
    // 等待第三个定时器
    core->printf(NULL, "Waiting for third timer (500ms)...\n");
    err = InfraxMuxClass.pollall(NULL, 0, sequence_timer_handler, &ctx, 500);  // 增加等待时间
    if (test_timeout || (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT)) {
        core->printf(NULL, "Third sequence wait failed: %s\n", err.message);
        InfraxMuxClass.clearTimeout(timer1);
        InfraxMuxClass.clearTimeout(timer2);
        InfraxMuxClass.clearTimeout(timer3);
        clear_timeout();
        return;
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
    
    return 0;
}
