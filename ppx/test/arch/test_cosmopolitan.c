#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxLog.h"
#include "internal/infrax/cosmopolitan.h"

// Define itimerspec structure if not defined
#ifndef _STRUCT_ITIMERSPEC
#define _STRUCT_ITIMERSPEC
struct itimerspec {
    struct timespec it_interval;  // Timer interval
    struct timespec it_value;     // Initial expiration
};
#endif

// Forward declarations for timerfd functions
int sys_timerfd_create(int clockid, int flags);
int sys_timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);

InfraxCore* core = NULL;
const PpxInfra* infra = NULL;

static void test_timerfd(void) {
    core->printf(core, "Testing timerfd availability...\n");
    
    // Try to create a timerfd using syscall
    int fd = sys_timerfd_create(CLOCK_MONOTONIC, 0);
    if (fd == -1) {
        core->printf(core, "timerfd_create failed with errno=%d\n", errno);
        return;
    }
    
    // Set up timer to fire after 100ms
    struct itimerspec new_value = {
        .it_value = {.tv_sec = 0, .tv_nsec = 100000000},  // 100ms
        .it_interval = {.tv_sec = 0, .tv_nsec = 0}
    };
    
    if (sys_timerfd_settime(fd, 0, &new_value, NULL) == -1) {
        core->printf(core, "timerfd_settime failed with errno=%d\n", errno);
        close(fd);
        return;
    }
    
    // Wait for timer to fire
    uint64_t exp;
    ssize_t s = read(fd, &exp, sizeof(uint64_t));
    if (s != sizeof(uint64_t)) {
        core->printf(core, "read failed with errno=%d\n", errno);
    } else {
        core->printf(core, "Timer successfully fired %lu times\n", exp);
    }
    
    close(fd);
    core->printf(core, "timerfd test completed\n");
}

int main(void) {
    infra = ppx_infra();
    core = infra->core;  // Use core from infra instead of _infrax_core_singleton
    
    INFRAX_ASSERT(core, core != NULL);
    INFRAX_ASSERT(core, infra != NULL);
    
    test_timerfd();
    
    return 0;
} 