#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxTimer.h"
#include "internal/infrax/InfraxMux.h"

InfraxCore* core;
static void timer_handler(int fd, short events, void* arg) {
    core->printf(NULL,"Timer event received!\n");
    *(int*)arg = 1;  // Set result
}

void test_mux_timer() {
    core->printf(NULL,"Testing mux with timer thread...\n");
    
    // Set timeout
    int result = 0;
    InfraxU32 timer_id = InfraxMuxClass.setTimeout(2000, timer_handler, &result);
    if (timer_id == 0) {
        core->printf(NULL,"Failed to set timeout\n");
        return;
    }
    
    // Wait for timer event
    InfraxError err = InfraxMuxClass.pollall(NULL, 0, timer_handler, &result, 3000);
    if (err.code != 0 && err.code != INFRAX_ERROR_TIMEOUT) {
        core->printf(NULL,"Poll failed: %s\n", err.message);
        return;
    }
    
    if (!result) {
        core->printf(NULL,"Timer did not expire in time\n");
        return;
    }
    
    core->printf(NULL,"Timer test passed\n");
    core->printf(NULL,"Timer test completed successfully\n");
}

int main() {
    // Get core singleton
    core = InfraxCoreClass.singleton();
    test_mux_timer();
    return 0;
}
