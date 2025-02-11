#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"

static InfraxCore* core = NULL;

// Test async function
static void test_async_fn(InfraxAsync* self, void* arg) {
    core->printf(core, "Test async function started\n");
    InfraxAsyncClass.yield(self);
    core->printf(core, "Test async function resumed\n");
}

// Test poll callback
static void test_poll_callback(int fd, short revents, void* arg) {
    if (revents & INFRAX_POLLIN) {
        char buf[128];
        ssize_t n = core->read_fd(core, fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            core->printf(core, "Poll callback received: %s\n", buf);
        }
    }
}

int main(void) {
    core = InfraxCoreClass.singleton();
    core->printf(core, "\n=== Testing InfraxAsync ===\n\n");
    
    // Test 1: Basic async task
    core->printf(core, "Test 1: Basic async task\n");
    InfraxAsync* async = InfraxAsyncClass.new(test_async_fn, NULL);
    if (!async) {
        core->printf(core, "Failed to create async task\n");
        return 1;
    }
    
    InfraxAsyncClass.start(async);
    core->printf(core, "Async task started\n");
    
    InfraxAsyncClass.start(async);  // Resume
    core->printf(core, "Async task completed\n");
    
    // Test 2: Pollset
    core->printf(core, "\nTest 2: Pollset\n");
    
    // Create pipe for testing
    int pipefd[2];
    if (core->create_pipe(core, pipefd) != 0) {
        core->printf(core, "Failed to create pipe\n");
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Set non-blocking
    core->set_nonblocking(core, pipefd[0]);
    core->set_nonblocking(core, pipefd[1]);
    
    // Add read end to pollset
    if (InfraxAsyncClass.pollset_add_fd(async, pipefd[0], INFRAX_POLLIN, test_poll_callback, NULL) != 0) {
        core->printf(core, "Failed to add fd to pollset\n");
        core->close_fd(core, pipefd[0]);
        core->close_fd(core, pipefd[1]);
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Write some data
    const char* test_data = "Hello, Poll!";
    core->write_fd(core, pipefd[1], test_data, core->strlen(core, test_data));
    
    // Poll for events
    core->printf(core, "Polling for events...\n");
    InfraxAsyncClass.pollset_poll(async, 1000);
    
    // Remove fd from pollset
    InfraxAsyncClass.pollset_remove_fd(async, pipefd[0]);
    
    // Cleanup
    core->close_fd(core, pipefd[0]);
    core->close_fd(core, pipefd[1]);
    InfraxAsyncClass.free(async);
    
    core->printf(core, "\n=== All infrax_async tests completed ===\n");
    return 0;
}
