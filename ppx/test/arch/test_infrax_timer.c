#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"

static int timer_called = 0;
static InfraxCore* core = NULL;

static void timer_callback(void* arg) {
    timer_called = 1;
    core->printf(core, "Timer callback called with arg: %p\n", arg);
}

static void async_task(InfraxAsync* self, void* arg) {
    core->printf(core, "Task started\n");
    
    // Add a timer for 100ms
    int ret = InfraxAsyncClass.add_timer(self, 100, timer_callback, arg);
    INFRAX_ASSERT(core, ret == 0);
    
    // Yield to let timer run
    InfraxAsyncClass.yield(self);
    
    core->printf(core, "Task resumed after timer\n");
}

int main() {
    core = InfraxCoreClass.singleton();
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
    
    core->printf(core, "Test completed successfully\n");
    return 0;
}
