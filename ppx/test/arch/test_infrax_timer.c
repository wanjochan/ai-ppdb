#include "internal/infrax/InfraxAsync.h"
#include <stdio.h>
#include <assert.h>

static int timer_called = 0;

static void timer_callback(void* arg) {
    timer_called = 1;
    printf("Timer callback called with arg: %p\n", arg);
}

static void async_task(InfraxAsync* self, void* arg) {
    printf("Task started\n");
    
    // Add a timer for 100ms
    int ret = InfraxAsyncClass.add_timer(self, 100, timer_callback, arg);
    assert(ret == 0);
    
    // Yield to let timer run
    InfraxAsyncClass.yield(self);
    
    printf("Task resumed after timer\n");
}

int main() {
    // Initialize scheduler
    infrax_scheduler_init();
    
    // Create and start task
    InfraxAsync* task = InfraxAsyncClass.new(async_task, NULL);
    task = InfraxAsyncClass.start(task);
    
    // Poll scheduler until timer fires
    while (!timer_called) {
        infrax_scheduler_poll();
    }
    
    // Cleanup
    InfraxAsyncClass.free(task);
    
    printf("Test completed successfully\n");
    return 0;
}
