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
#define TEST_TIMEOUT_MS 2000  // 2 seconds timeout for each test

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

int main() {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    int test_result = 0;  // 用于跟踪测试结果
    
    core->printf(core, "\n=== Testing PolyxAsync ===\n\n");
    
    // Create PolyxAsync instance
    PolyxAsync* async = PolyxAsyncClass.new();
    INFRAX_ASSERT(core, async != NULL);
    
    // Test 1: Timer
    core->printf(core, "Test 1: Timer\n");
    int timer_count = 0;
    int expected_timer_count = 2;  // 期望定时器触发2次
    
    PolyxTimerConfig timer_config = {
        .interval_ms = 1000,
        .callback = test_timer_callback,
        .arg = &timer_count
    };
    
    PolyxEvent* timer = async->create_timer(async, &timer_config);
    INFRAX_ASSERT(core, timer != NULL);
    
    // Start timer
    core->printf(core, "Starting timer...\n");
    async->start_timer(async, timer);
    
    // Test 2: Custom Event
    core->printf(core, "\nTest 2: Custom Event\n");
    
    const char* event_data = "Custom Event Data";
    int event_trigger_count = 0;
    int event_callback_count = 0;
    
    PolyxEventConfig event_config = {
        .type = POLYX_EVENT_IO,
        .callback = test_event_callback,
        .arg = &event_callback_count
    };
    
    PolyxEvent* event = async->create_event(async, &event_config);
    INFRAX_ASSERT(core, event != NULL);
    
    // Poll loop
    core->printf(core, "\nStarting poll loop...\n");
    InfraxTime start_time = core->time_monotonic_ms(core);
    
    while (core->time_monotonic_ms(core) - start_time < TEST_TIMEOUT_MS) {
        // Trigger custom event every other iteration
        if (event_trigger_count < 2) {  // 只触发两次事件
            core->printf(core, "Triggering custom event...\n");
            async->trigger_event(async, event, (void*)event_data, core->strlen(core, event_data) + 1);
            event_trigger_count++;
        }
        
        // Poll for events
        async->poll(async, 50);  // 使用更短的轮询间隔
        
        // 检查是否达到预期结果
        if (timer_count >= expected_timer_count && event_callback_count >= event_trigger_count) {
            break;
        }
    }
    
    // 验证定时器结果
    core->printf(core, "\nVerifying timer results...\n");
    if (timer_count != expected_timer_count) {
        log->error(log, "Timer test failed: expected %d calls, got %d", 
                  expected_timer_count, timer_count);
        test_result = 1;
    }
    
    // 验证事件结果
    core->printf(core, "Verifying event results...\n");
    if (event_callback_count != event_trigger_count) {
        log->error(log, "Event test failed: triggered %d times, callback called %d times",
                  event_trigger_count, event_callback_count);
        test_result = 1;
    }
    
    // Stop timer
    core->printf(core, "\nStopping timer...\n");
    async->stop_timer(async, timer);
    
    // Cleanup
    async->destroy_event(async, event);
    async->destroy_event(async, timer);
    PolyxAsyncClass.free(async);
    
    if (test_result == 0) {
        core->printf(core, "\n=== All polyx_async tests PASSED ===\n");
    } else {
        core->printf(core, "\n=== Some polyx_async tests FAILED ===\n");
    }
    return test_result;
}
