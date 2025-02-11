#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxLog.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <errno.h>

InfraxCore* core = NULL;
const PpxInfra* infra = NULL;

static void test_timerfd(void) {
    core->printf(core, "Testing timerfd availability...\n");
    
    // Try to create a timerfd
    int fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (fd == -1) {
        core->printf(core, "timerfd_create failed with errno=%d\n", errno);
        return;
    }
    
    // Set up timer to fire after 100ms
    struct itimerspec new_value;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_nsec = 100000000; // 100ms
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;
    
    if (timerfd_settime(fd, 0, &new_value, NULL) == -1) {
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
    core = infrax_core_new();
    infra = ppx_infra_new();
    
    INFRAX_ASSERT(core, core != NULL);
    INFRAX_ASSERT(core, infra != NULL);
    
    test_timerfd();
    
    ppx_infra_free((PpxInfra*)infra);
    infrax_core_free(core);
    return 0;
} 