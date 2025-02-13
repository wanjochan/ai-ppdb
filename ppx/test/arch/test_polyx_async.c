#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/polyx/PolyxAsync.h"

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
    
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxHandle fd;
    InfraxError err = core->file_open(core, TEST_FILE, INFRAX_FILE_RDONLY, 0644, &fd);
    if (INFRAX_ERROR_IS_ERR(err)) {
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    char* buffer = (char*)arg;
    size_t total_read = 0;
    size_t bytes_read = 0;
    
    while (total_read < TEST_DATA_LEN && self->state == INFRAX_ASYNC_PENDING) {
        err = core->file_read(core, fd, buffer + total_read, TEST_DATA_LEN - total_read, &bytes_read);
        if (INFRAX_ERROR_IS_ERR(err)) {
            self->state = INFRAX_ASYNC_REJECTED;
            break;
        } else if (bytes_read == 0) {
            // EOF
            break;
        }
        total_read += bytes_read;
    }
    
    core->file_close(core, fd);
    
    if (self->state == INFRAX_ASYNC_PENDING) {
        self->state = INFRAX_ASYNC_FULFILLED;
    }
}

// Async write file implementation
static void async_write_file(InfraxAsync* self, void* arg) {
    if (!self || !arg) return;
    
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxHandle fd;
    InfraxError err = core->file_open(core, TEST_FILE, INFRAX_FILE_CREATE | INFRAX_FILE_WRONLY | INFRAX_FILE_TRUNC, 0644, &fd);
    if (INFRAX_ERROR_IS_ERR(err)) {
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    const char* data = (const char*)arg;
    size_t total_written = 0;
    size_t bytes_written = 0;
    
    while (total_written < TEST_DATA_LEN && self->state == INFRAX_ASYNC_PENDING) {
        err = core->file_write(core, fd, data + total_written, TEST_DATA_LEN - total_written, &bytes_written);
        if (INFRAX_ERROR_IS_ERR(err)) {
            self->state = INFRAX_ASYNC_REJECTED;
            break;
        }
        total_written += bytes_written;
    }
    
    core->file_close(core, fd);
    
    if (self->state == INFRAX_ASYNC_PENDING) {
        self->state = INFRAX_ASYNC_FULFILLED;
    }
}

// Test async write file
static void test_polyx_async_write_file(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "Testing async write file...\n");
    
    // Create async task
    InfraxAsync* async = InfraxAsyncClass.new(async_write_file, (void*)TEST_DATA);
    INFRAX_ASSERT(core, async != NULL);
    
    // Start task
    bool started = InfraxAsyncClass.start(async);
    INFRAX_ASSERT(core, started);
    
    // Poll until done
    while (!InfraxAsyncClass.is_done(async)) {
        int ret = InfraxAsyncClass.pollset_poll(async, 100);  // 100ms timeout
        INFRAX_ASSERT(core, ret >= 0);
    }
    
    // Check result
    INFRAX_ASSERT(core, async->state == INFRAX_ASYNC_FULFILLED);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    
    core->printf(core, "Async write file test passed\n");
}

// Test async read file
static void test_polyx_async_read_file(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "Testing async read file...\n");
    
    // Create buffer for read data
    char buffer[TEST_DATA_LEN + 1] = {0};
    
    // Create async task
    InfraxAsync* async = InfraxAsyncClass.new(async_read_file, buffer);
    INFRAX_ASSERT(core, async != NULL);
    
    // Start task
    bool started = InfraxAsyncClass.start(async);
    INFRAX_ASSERT(core, started);
    
    // Poll until done
    while (!InfraxAsyncClass.is_done(async)) {
        int ret = InfraxAsyncClass.pollset_poll(async, 100);  // 100ms timeout
        INFRAX_ASSERT(core, ret >= 0);
    }
    
    // Check result
    INFRAX_ASSERT(core, async->state == INFRAX_ASYNC_FULFILLED);
    INFRAX_ASSERT(core, core->strcmp(core, buffer, TEST_DATA) == 0);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    
    core->printf(core, "Async read file test passed\n");
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
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "[%s:%d] %s: %s\n", file, line, func, msg);
}

// Basic functionality tests
void test_polyx_async_basic(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    INFRAX_ASSERT(core, async != NULL);
    core->printf(core, "PolyxAsync instance created successfully\n");
    
    // Test event creation
    core->printf(core, "Creating event configuration...\n");
    PolyxEventConfig config = {
        .type = POLYX_EVENT_IO,
        .callback = NULL,
        .arg = NULL
    };
    
    core->printf(core, "Creating event...\n");
    PolyxEvent* event = PolyxAsyncClass.create_event(async, &config);
    INFRAX_ASSERT(core, event != NULL);
    core->printf(core, "Event created successfully\n");
    
    core->printf(core, "Checking event properties...\n");
    INFRAX_ASSERT(core, event->type == POLYX_EVENT_IO);
    INFRAX_ASSERT(core, event->status == POLYX_EVENT_STATUS_INIT);
    core->printf(core, "Event properties verified\n");
    
    core->printf(core, "Destroying event...\n");
    PolyxAsyncClass.destroy_event(async, event);
    core->printf(core, "Event destroyed successfully\n");
    
    core->printf(core, "Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    core->printf(core, "PolyxAsync instance freed successfully\n");
}

// Network event tests
void test_polyx_async_network(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "\nStarting network tests...\n");
    
    core->printf(core, "Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    INFRAX_ASSERT(core, async != NULL);
    core->printf(core, "PolyxAsync instance created successfully\n");
    
    // Test TCP event
    core->printf(core, "Creating TCP event configuration...\n");
    PolyxNetworkConfig tcp_config = {
        .socket_fd = -1,
        .events = POLLIN | POLLOUT,
        .protocol_opts.tcp = {
            .backlog = 5,
            .reuse_addr = true
        }
    };
    
    core->printf(core, "Creating TCP event...\n");
    PolyxEvent* tcp_event = PolyxAsyncClass.create_tcp_event(async, &tcp_config);
    INFRAX_ASSERT(core, tcp_event != NULL);
    core->printf(core, "TCP event created successfully\n");
    
    core->printf(core, "Checking TCP event properties...\n");
    INFRAX_ASSERT(core, POLYX_EVENT_IS_NETWORK(tcp_event));
    core->printf(core, "TCP event properties verified\n");
    
    core->printf(core, "Destroying TCP event...\n");
    PolyxAsyncClass.destroy_event(async, tcp_event);
    core->printf(core, "TCP event destroyed successfully\n");
    
    core->printf(core, "Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    core->printf(core, "PolyxAsync instance freed successfully\n");
}

// Debug functionality tests
void test_polyx_async_debug(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "\nStarting debug tests...\n");
    
    core->printf(core, "Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    INFRAX_ASSERT(core, async != NULL);
    core->printf(core, "PolyxAsync instance created successfully\n");
    
    core->printf(core, "Setting debug level and callback...\n");
    PolyxAsyncClass.set_debug_level(async, POLYX_DEBUG_INFO);
    PolyxAsyncClass.set_debug_callback(async, test_debug_callback, NULL);
    core->printf(core, "Debug settings configured\n");
    
    core->printf(core, "Testing debug message...\n");
    POLYX_INFO(async, "Debug test message");
    core->printf(core, "Debug message sent\n");
    
    core->printf(core, "Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    core->printf(core, "PolyxAsync instance freed successfully\n");
}

// Event statistics tests
void test_polyx_async_stats(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "\nStarting statistics tests...\n");
    
    core->printf(core, "Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    INFRAX_ASSERT(core, async != NULL);
    core->printf(core, "PolyxAsync instance created successfully\n");
    
    core->printf(core, "Getting initial statistics...\n");
    PolyxEventStats stats;
    PolyxAsyncClass.get_stats(async, &stats);
    INFRAX_ASSERT(core, stats.total_events == 0);
    INFRAX_ASSERT(core, stats.active_events == 0);
    core->printf(core, "Initial statistics verified\n");
    
    // Create some events
    core->printf(core, "Creating test events...\n");
    PolyxEventConfig config = {
        .type = POLYX_EVENT_IO,
        .callback = NULL,
        .arg = NULL
    };
    PolyxEvent* event1 = PolyxAsyncClass.create_event(async, &config);
    PolyxEvent* event2 = PolyxAsyncClass.create_event(async, &config);
    INFRAX_ASSERT(core, event1 != NULL && event2 != NULL);
    core->printf(core, "Test events created successfully\n");
    
    core->printf(core, "Getting updated statistics...\n");
    PolyxAsyncClass.get_stats(async, &stats);
    INFRAX_ASSERT(core, stats.total_events == 2);
    core->printf(core, "Updated statistics verified\n");
    
    core->printf(core, "Cleaning up events...\n");
    PolyxAsyncClass.destroy_event(async, event1);
    PolyxAsyncClass.destroy_event(async, event2);
    core->printf(core, "Events cleaned up successfully\n");
    
    core->printf(core, "Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    core->printf(core, "PolyxAsync instance freed successfully\n");
}

// Event group tests
void test_polyx_async_group(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "\nStarting event group tests...\n");
    
    core->printf(core, "Creating new PolyxAsync instance...\n");
    PolyxAsync* async = PolyxAsyncClass.new();
    INFRAX_ASSERT(core, async != NULL);
    core->printf(core, "PolyxAsync instance created successfully\n");
    
    // Create events
    core->printf(core, "Creating test events...\n");
    PolyxEventConfig config = {
        .type = POLYX_EVENT_IO,
        .callback = NULL,
        .arg = NULL
    };
    PolyxEvent* events[2];
    events[0] = PolyxAsyncClass.create_event(async, &config);
    events[1] = PolyxAsyncClass.create_event(async, &config);
    INFRAX_ASSERT(core, events[0] != NULL && events[1] != NULL);
    core->printf(core, "Test events created successfully\n");
    
    // Create event group
    core->printf(core, "Creating event group...\n");
    int group_id = PolyxAsyncClass.create_event_group(async, events, 2);
    INFRAX_ASSERT(core, group_id >= 0);
    core->printf(core, "Event group created successfully\n");
    
    // Test wait
    core->printf(core, "Testing event group wait...\n");
    int ret = PolyxAsyncClass.wait_event_group(async, group_id, 0);
    INFRAX_ASSERT(core, ret == POLYX_ERROR_TIMEOUT);
    core->printf(core, "Event group wait test passed\n");
    
    core->printf(core, "Cleaning up...\n");
    PolyxAsyncClass.destroy_event_group(async, group_id);
    PolyxAsyncClass.destroy_event(async, events[0]);
    PolyxAsyncClass.destroy_event(async, events[1]);
    core->printf(core, "Event group and events cleaned up successfully\n");
    
    core->printf(core, "Freeing PolyxAsync instance...\n");
    PolyxAsyncClass.free(async);
    core->printf(core, "PolyxAsync instance freed successfully\n");
}

int main(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "\n=== Running PolyxAsync tests ===\n\n");
    
    core->printf(core, "Running basic tests...\n");
    test_polyx_async_basic();
    core->printf(core, "Basic tests passed\n\n");
    
    /*
    core->printf(core, "Running network tests...\n");
    test_polyx_async_network();
    core->printf(core, "Network tests passed\n\n");
    
    core->printf(core, "Running debug tests...\n");
    test_polyx_async_debug();
    core->printf(core, "Debug tests passed\n\n");
    
    core->printf(core, "Running statistics tests...\n");
    test_polyx_async_stats();
    core->printf(core, "Statistics tests passed\n\n");
    
    core->printf(core, "Running event group tests...\n");
    test_polyx_async_group();
    core->printf(core, "Event group tests passed\n\n");
    */
    
    core->printf(core, "=== All tests passed! ===\n\n");
    return 0;
}
