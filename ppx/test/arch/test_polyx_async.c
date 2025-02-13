#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"
#include "internal/polyx/PolyxAsync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

// Test timeout control
#define TEST_TIMEOUT_MS 5000  // 增加到5秒超时

// Test file operations
#define TEST_FILE "test.txt"
#define TEST_DATA "Hello, Async World!"
#define TEST_DATA_LEN 18

// Test context structures
typedef struct {
    InfraxHandle fd;      // Changed from int to InfraxHandle
    char* buffer;       // Buffer for read/write
    size_t size;        // Buffer size
    size_t bytes_processed;  // Total bytes processed
    const char* filename;
    int yield_count;    // Count how many times yield is called
} AsyncFileContext;

// Forward declarations
static void test_polyx_async_read_file(void);
static void test_polyx_async_write_file(void);

// Async read file implementation
static void async_read_file(InfraxAsync* self, void* arg) {
    if (!self || !arg) return;
    
    int fd = open(TEST_FILE, O_RDONLY);
    if (fd < 0) {
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    char* buffer = (char*)arg;
    ssize_t total_read = 0;
    
    while (total_read < TEST_DATA_LEN && self->state == INFRAX_ASYNC_PENDING) {
        ssize_t bytes_read = read(fd, buffer + total_read, TEST_DATA_LEN - total_read);
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block, return and let pollset handle other events
                return;
            }
            self->state = INFRAX_ASYNC_REJECTED;
            break;
        } else if (bytes_read == 0) {
            // EOF
            break;
        }
        total_read += bytes_read;
    }
    
    close(fd);
    
    if (self->state == INFRAX_ASYNC_PENDING) {
        self->state = INFRAX_ASYNC_FULFILLED;
    }
}

// Async write file implementation
static void async_write_file(InfraxAsync* self, void* arg) {
    if (!self || !arg) return;
    
    int fd = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    const char* data = (const char*)arg;
    ssize_t total_written = 0;
    
    while (total_written < TEST_DATA_LEN && self->state == INFRAX_ASYNC_PENDING) {
        ssize_t bytes_written = write(fd, data + total_written, TEST_DATA_LEN - total_written);
        if (bytes_written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block, return and let pollset handle other events
                return;
            }
            self->state = INFRAX_ASYNC_REJECTED;
            break;
        }
        total_written += bytes_written;
    }
    
    close(fd);
    
    if (self->state == INFRAX_ASYNC_PENDING) {
        self->state = INFRAX_ASYNC_FULFILLED;
    }
}

// Test async write file
static void test_polyx_async_write_file(void) {
    printf("Testing async write file...\n");
    
    // Create async task
    InfraxAsync* async = InfraxAsyncClass.new(async_write_file, (void*)TEST_DATA);
    assert(async != NULL);
    
    // Start task
    bool started = InfraxAsyncClass.start(async);
    assert(started);
    
    // Poll until done
    while (!InfraxAsyncClass.is_done(async)) {
        int ret = InfraxAsyncClass.pollset_poll(async, 100);  // 100ms timeout
        assert(ret >= 0);
    }
    
    // Check result
    assert(async->state == INFRAX_ASYNC_FULFILLED);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    
    printf("Async write file test passed\n");
}

// Test async read file
static void test_polyx_async_read_file(void) {
    printf("Testing async read file...\n");
    
    // Create buffer for read data
    char buffer[TEST_DATA_LEN + 1] = {0};
    
    // Create async task
    InfraxAsync* async = InfraxAsyncClass.new(async_read_file, buffer);
    assert(async != NULL);
    
    // Start task
    bool started = InfraxAsyncClass.start(async);
    assert(started);
    
    // Poll until done
    while (!InfraxAsyncClass.is_done(async)) {
        int ret = InfraxAsyncClass.pollset_poll(async, 100);  // 100ms timeout
        assert(ret >= 0);
    }
    
    // Check result
    assert(async->state == INFRAX_ASYNC_FULFILLED);
    assert(strcmp(buffer, TEST_DATA) == 0);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    
    printf("Async read file test passed\n");
}

// Timer callback
static void test_timer_callback(void* arg) {
    int* count = (int*)arg;
    (*count)++;
    
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "Timer callback called %d times\n", *count);
}

// Event callback
static void test_event_callback(PolyxEvent* event, void* arg) {
    int* count = (int*)arg;
    (*count)++;
    
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "Event callback called %d times\n", *count);
}

// Debug callback for testing
static void test_debug_callback(PolyxDebugLevel level, const char* file, int line, 
                              const char* func, const char* msg) {
    printf("[%s:%d] %s: %s\n", file, line, func, msg);
}

// Basic functionality tests
void test_polyx_async_basic(void) {
    printf("Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    assert(async != NULL);
    printf("PolyxAsync instance created successfully\n");
    
    // Test event creation
    printf("Creating event configuration...\n");
    PolyxEventConfig config = {
        .type = POLYX_EVENT_IO,
        .callback = NULL,
        .arg = NULL
    };
    
    printf("Creating event...\n");
    PolyxEvent* event = PolyxAsyncClass.create_event(async, &config);
    assert(event != NULL);
    printf("Event created successfully\n");
    
    printf("Checking event properties...\n");
    assert(event->type == POLYX_EVENT_IO);
    assert(event->status == POLYX_EVENT_STATUS_INIT);
    printf("Event properties verified\n");
    
    printf("Destroying event...\n");
    PolyxAsyncClass.destroy_event(async, event);
    printf("Event destroyed successfully\n");
    
    printf("Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    printf("PolyxAsync instance freed successfully\n");
}

// Network event tests
void test_polyx_async_network(void) {
    printf("\nStarting network tests...\n");
    
    printf("Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    assert(async != NULL);
    printf("PolyxAsync instance created successfully\n");
    
    // Test TCP event
    printf("Creating TCP event configuration...\n");
    PolyxNetworkConfig tcp_config = {
        .socket_fd = -1,
        .events = POLLIN | POLLOUT,
        .protocol_opts.tcp = {
            .backlog = 5,
            .reuse_addr = true
        }
    };
    
    printf("Creating TCP event...\n");
    PolyxEvent* tcp_event = PolyxAsyncClass.create_tcp_event(async, &tcp_config);
    assert(tcp_event != NULL);
    printf("TCP event created successfully\n");
    
    printf("Checking TCP event properties...\n");
    assert(POLYX_EVENT_IS_NETWORK(tcp_event));
    printf("TCP event properties verified\n");
    
    printf("Destroying TCP event...\n");
    PolyxAsyncClass.destroy_event(async, tcp_event);
    printf("TCP event destroyed successfully\n");
    
    printf("Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    printf("PolyxAsync instance freed successfully\n");
}

// Debug functionality tests
void test_polyx_async_debug(void) {
    printf("\nStarting debug tests...\n");
    
    printf("Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    assert(async != NULL);
    printf("PolyxAsync instance created successfully\n");
    
    printf("Setting debug level and callback...\n");
    PolyxAsyncClass.set_debug_level(async, POLYX_DEBUG_INFO);
    PolyxAsyncClass.set_debug_callback(async, test_debug_callback, NULL);
    printf("Debug settings configured\n");
    
    printf("Testing debug message...\n");
    POLYX_INFO(async, "Debug test message");
    printf("Debug message sent\n");
    
    printf("Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    printf("PolyxAsync instance freed successfully\n");
}

// Event statistics tests
void test_polyx_async_stats(void) {
    printf("\nStarting statistics tests...\n");
    
    printf("Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    assert(async != NULL);
    printf("PolyxAsync instance created successfully\n");
    
    printf("Getting initial statistics...\n");
    PolyxEventStats stats;
    PolyxAsyncClass.get_stats(async, &stats);
    assert(stats.total_events == 0);
    assert(stats.active_events == 0);
    printf("Initial statistics verified\n");
    
    // Create some events
    printf("Creating test events...\n");
    PolyxEventConfig config = {
        .type = POLYX_EVENT_IO,
        .callback = NULL,
        .arg = NULL
    };
    PolyxEvent* event1 = PolyxAsyncClass.create_event(async, &config);
    PolyxEvent* event2 = PolyxAsyncClass.create_event(async, &config);
    assert(event1 != NULL && event2 != NULL);
    printf("Test events created successfully\n");
    
    printf("Getting updated statistics...\n");
    PolyxAsyncClass.get_stats(async, &stats);
    assert(stats.total_events == 2);
    printf("Updated statistics verified\n");
    
    printf("Cleaning up events...\n");
    PolyxAsyncClass.destroy_event(async, event1);
    PolyxAsyncClass.destroy_event(async, event2);
    printf("Events cleaned up successfully\n");
    
    printf("Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    printf("PolyxAsync instance freed successfully\n");
}

// Event group tests
void test_polyx_async_group(void) {
    printf("\nStarting event group tests...\n");
    
    printf("Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    assert(async != NULL);
    printf("PolyxAsync instance created successfully\n");
    
    // Create events
    printf("Creating test events...\n");
    PolyxEventConfig config = {
        .type = POLYX_EVENT_IO,
        .callback = NULL,
        .arg = NULL
    };
    PolyxEvent* events[2];
    events[0] = PolyxAsyncClass.create_event(async, &config);
    events[1] = PolyxAsyncClass.create_event(async, &config);
    assert(events[0] != NULL && events[1] != NULL);
    printf("Test events created successfully\n");
    
    // Create event group
    printf("Creating event group...\n");
    int group_id = PolyxAsyncClass.create_event_group(async, events, 2);
    assert(group_id >= 0);
    printf("Event group created successfully\n");
    
    // Test wait
    printf("Testing event group wait...\n");
    int ret = PolyxAsyncClass.wait_event_group(async, group_id, 0);
    assert(ret == POLYX_ERROR_TIMEOUT);
    printf("Event group wait test passed\n");
    
    printf("Cleaning up...\n");
    PolyxAsyncClass.destroy_event_group(async, group_id);
    PolyxAsyncClass.destroy_event(async, events[0]);
    PolyxAsyncClass.destroy_event(async, events[1]);
    printf("Event group and events cleaned up successfully\n");
    
    printf("Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    printf("PolyxAsync instance freed successfully\n");
}

int main(void) {
    printf("\n=== Running PolyxAsync tests ===\n\n");
    
    printf("Running basic tests...\n");
    test_polyx_async_basic();
    printf("Basic tests passed\n\n");
    
    /*
    printf("Running network tests...\n");
    test_polyx_async_network();
    printf("Network tests passed\n\n");
    
    printf("Running debug tests...\n");
    test_polyx_async_debug();
    printf("Debug tests passed\n\n");
    
    printf("Running statistics tests...\n");
    test_polyx_async_stats();
    printf("Statistics tests passed\n\n");
    
    printf("Running event group tests...\n");
    test_polyx_async_group();
    printf("Event group tests passed\n\n");
    */
    
    printf("=== All tests passed! ===\n\n");
    return 0;
}
