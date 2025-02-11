#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Test async function
static void test_async_fn(InfraxAsync* self, void* arg) {
    printf("Test async function started\n");
    InfraxAsyncClass.yield(self);
    printf("Test async function resumed\n");
}

// Test poll callback
static void test_poll_callback(int fd, short revents, void* arg) {
    if (revents & INFRAX_POLLIN) {
        char buf[128];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Poll callback received: %s\n", buf);
        }
    }
}

int main(void) {
    printf("\n=== Testing InfraxAsync ===\n\n");
    
    // Test 1: Basic async task
    printf("Test 1: Basic async task\n");
    InfraxAsync* async = InfraxAsyncClass.new(test_async_fn, NULL);
    if (!async) {
        printf("Failed to create async task\n");
        return 1;
    }
    
    InfraxAsyncClass.start(async);
    printf("Async task started\n");
    
    InfraxAsyncClass.start(async);  // Resume
    printf("Async task completed\n");
    
    // Test 2: Pollset
    printf("\nTest 2: Pollset\n");
    
    // Create pipe for testing
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        printf("Failed to create pipe\n");
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Set non-blocking
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    
    // Add read end to pollset
    if (InfraxAsyncClass.pollset_add_fd(async, pipefd[0], INFRAX_POLLIN, test_poll_callback, NULL) != 0) {
        printf("Failed to add fd to pollset\n");
        close(pipefd[0]);
        close(pipefd[1]);
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Write some data
    const char* test_data = "Hello, Poll!";
    write(pipefd[1], test_data, strlen(test_data));
    
    // Poll for events
    printf("Polling for events...\n");
    InfraxAsyncClass.pollset_poll(async, 1000);
    
    // Remove fd from pollset
    InfraxAsyncClass.pollset_remove_fd(async, pipefd[0]);
    
    // Cleanup
    close(pipefd[0]);
    close(pipefd[1]);
    InfraxAsyncClass.free(async);
    
    printf("\n=== All tests completed ===\n");
    return 0;
}
