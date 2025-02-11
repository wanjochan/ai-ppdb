#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"
#include "internal/polyx/PolyxAsync.h"

// Test timeout control
#define TEST_TIMEOUT_MS 2000  // 2 seconds timeout for each test

// Test context structures
typedef struct {
    InfraxHandle fd;      // Changed from int to InfraxHandle
    char* buffer;       // Buffer for read/write
    size_t size;        // Buffer size
    size_t bytes_processed;  // Total bytes processed
    const char* filename;
    int yield_count;    // Count how many times yield is called
} AsyncFileContext;

// Async read file function
static void async_read_file(InfraxAsync* self, void* arg) {
    AsyncFileContext* ctx = (AsyncFileContext*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    
    if (!ctx) {
        log->debug(log, "async_read_file: ctx is NULL");
        return;
    }
    
    // Open file if not already open
    if (ctx->fd == 0) {  // Changed from < 0 to == 0 since InfraxHandle is unsigned
        log->debug(log, "async_read_file: opening file %s", ctx->filename);
        InfraxError err = core->file_open(core, ctx->filename, INFRAX_FILE_RDONLY, 0, &ctx->fd);
        if (!INFRAX_ERROR_IS_OK(err) || ctx->fd == 0) {
            log->debug(log, "async_read_file: failed to open file");
            self->state = INFRAX_ASYNC_REJECTED;
            return;
        }
    }
    
    // Try to read
    size_t bytes_read = 0;
    InfraxError err = core->file_read(core, ctx->fd, 
                                     ctx->buffer + ctx->bytes_processed,
                                     ctx->size - ctx->bytes_processed,
                                     &bytes_read);
    
    log->debug(log, "async_read_file: read returned %zu bytes", bytes_read);
    
    if (INFRAX_ERROR_IS_OK(err)) {
        if (bytes_read > 0) {
            ctx->bytes_processed += bytes_read;
            log->debug(log, "async_read_file: total bytes read: %zu/%zu", 
                   ctx->bytes_processed, ctx->size);
            if (ctx->bytes_processed < ctx->size) {
                ctx->yield_count++;
                log->debug(log, "async_read_file: yielding after successful read");
                InfraxAsyncClass.yield(self);
            } else {
                core->file_close(core, ctx->fd);
                ctx->fd = 0;  // Changed from -1 to 0
                self->state = INFRAX_ASYNC_FULFILLED;
            }
        } else {
            // End of file
            log->debug(log, "async_read_file: reached EOF");
            core->file_close(core, ctx->fd);
            ctx->fd = 0;  // Changed from -1 to 0
            self->state = INFRAX_ASYNC_FULFILLED;
        }
    } else {
        // Error occurred
        log->debug(log, "async_read_file: read error");
        core->file_close(core, ctx->fd);
        ctx->fd = 0;  // Changed from -1 to 0
        self->state = INFRAX_ASYNC_REJECTED;
    }
}

// Async write file function
static void async_write_file(InfraxAsync* self, void* arg) {
    AsyncFileContext* ctx = (AsyncFileContext*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    
    if (!ctx) {
        log->debug(log, "async_write_file: ctx is NULL");
        return;
    }
    
    // Open file if not already open
    if (ctx->fd == 0) {  // Changed from < 0 to == 0
        log->debug(log, "async_write_file: opening file %s", ctx->filename);
        InfraxError err = core->file_open(core, ctx->filename, 
                                 INFRAX_FILE_CREATE | INFRAX_FILE_WRONLY | INFRAX_FILE_TRUNC,
                                 0644, &ctx->fd);
        if (!INFRAX_ERROR_IS_OK(err) || ctx->fd == 0) {
            log->debug(log, "async_write_file: failed to open file");
            self->state = INFRAX_ASYNC_REJECTED;
            return;
        }
    }
    
    // Try to write
    size_t bytes_written = 0;
    InfraxError err = core->file_write(core, ctx->fd,
                                      ctx->buffer + ctx->bytes_processed,
                                      ctx->size - ctx->bytes_processed,
                                      &bytes_written);
    
    log->debug(log, "async_write_file: write returned %zu bytes", bytes_written);
    
    if (INFRAX_ERROR_IS_OK(err)) {
        if (bytes_written > 0) {
            ctx->bytes_processed += bytes_written;
            log->debug(log, "async_write_file: total bytes written: %zu/%zu", 
                   ctx->bytes_processed, ctx->size);
            if (ctx->bytes_processed < ctx->size) {
                ctx->yield_count++;
                log->debug(log, "async_write_file: yielding after successful write");
                InfraxAsyncClass.yield(self);
            } else {
                core->file_close(core, ctx->fd);
                ctx->fd = 0;  // Changed from -1 to 0
                self->state = INFRAX_ASYNC_FULFILLED;
            }
        } else {
            // Error occurred
            log->debug(log, "async_write_file: write error");
            core->file_close(core, ctx->fd);
            ctx->fd = 0;  // Changed from -1 to 0
            self->state = INFRAX_ASYNC_REJECTED;
        }
    }
}

// Test async file operations
void test_polyx_async_read_file(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "test_polyx_async_read_file: starting");
    
    const char* test_file = "test.txt";
    const char* test_data = "Hello, World!";
    
    // Create a test file
    InfraxHandle fp;
    InfraxError err = core->file_open(core, test_file, 
                                     INFRAX_FILE_CREATE | INFRAX_FILE_WRONLY | INFRAX_FILE_TRUNC,
                                     0644, &fp);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    INFRAX_ASSERT(core, fp != 0);
    
    size_t bytes_written = 0;
    err = core->file_write(core, fp, test_data, core->strlen(core, test_data), &bytes_written);
    INFRAX_ASSERT(core, INFRAX_ERROR_IS_OK(err));
    core->file_close(core, fp);
    
    log->debug(log, "test_polyx_async_read_file: test file created");
    
    // Prepare read context
    char buffer[128] = {0};
    AsyncFileContext ctx = {
        .fd = 0,  // Changed from -1 to 0
        .buffer = buffer,
        .size = sizeof(buffer),
        .bytes_processed = 0,
        .filename = test_file,
        .yield_count = 0
    };
    
    // Create and start async task
    log->debug(log, "test_polyx_async_read_file: creating async task");
    InfraxAsync* async = InfraxAsyncClass.new(async_read_file, &ctx);
    INFRAX_ASSERT(core, async != NULL);
    
    log->debug(log, "test_polyx_async_read_file: starting async task");
    InfraxAsyncClass.start(async);
    
    // Wait for completion with timeout
    log->debug(log, "test_polyx_async_read_file: waiting for completion");
    InfraxTime last_status = 0;
    InfraxTime start_time = core->time_monotonic_ms(core);
    
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        // Check timeout
        if (core->time_monotonic_ms(core) - start_time > TEST_TIMEOUT_MS) {
            log->error(log, "test_polyx_async_read_file: timeout after %d ms", TEST_TIMEOUT_MS);
            async->state = INFRAX_ASYNC_REJECTED;
            break;
        }
        
        if (async->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(async);
        }
        InfraxTime now = core->time_monotonic_ms(core);
        if (now - last_status >= 1000) {  // Log status every second
            log->debug(log, "test_polyx_async_read_file: waiting... (yield count: %d)", 
                        ctx.yield_count);
            last_status = now;
        }
        InfraxAsyncClass.yield(async);  // 使用异步 yield 替代阻塞的 sleep
    }
    
    log->info(log, "test_polyx_async_read_file: task completed");
    
    // Check result
    INFRAX_ASSERT(core, async->state == INFRAX_ASYNC_FULFILLED);
    INFRAX_ASSERT(core, core->strcmp(core, test_data, buffer) == 0);
    
    // Cleanup
    log->debug(log, "test_polyx_async_read_file: cleaning up");
    InfraxAsyncClass.free(async);
    core->file_remove(core, test_file);
    log->debug(log, "test_polyx_async_read_file: cleanup complete");
}

// Timer callback
static void test_timer_callback(void* arg) {
    int* count = (int*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "Timer triggered, count: %d\n", *count);
    (*count)++;
}

// Event callback
static void test_event_callback(PolyxEvent* event, void* arg) {
    if (!event || !arg) return;
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "Event triggered with data: %s\n", (const char*)arg);
}

int main() {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "\n=== Testing PolyxAsync ===\n\n");
    
    // Create PolyxAsync instance
    PolyxAsync* async = PolyxAsyncClass.new();
    if (!async) {
        core->printf(core, "Failed to create PolyxAsync instance\n");
        return 1;
    }
    
    // Test 1: Timer
    core->printf(core, "Test 1: Timer\n");
    int timer_count = 0;
    
    PolyxTimerConfig timer_config = {
        .interval_ms = 1000,
        .callback = test_timer_callback,
        .arg = &timer_count
    };
    
    PolyxEvent* timer = PolyxAsyncClass.create_timer(async, &timer_config);
    if (!timer) {
        core->printf(core, "Failed to create timer\n");
        PolyxAsyncClass.free(async);
        return 1;
    }
    
    // Start timer
    core->printf(core, "Starting timer...\n");
    PolyxAsyncClass.start_timer(async, timer);
    
    // Test 2: Custom Event
    core->printf(core, "\nTest 2: Custom Event\n");
    
    const char* event_data = "Custom Event Data";
    PolyxEventConfig event_config = {
        .type = POLYX_EVENT_IO,
        .callback = test_event_callback,
        .arg = (void*)event_data
    };
    
    PolyxEvent* event = PolyxAsyncClass.create_event(async, &event_config);
    if (!event) {
        core->printf(core, "Failed to create event\n");
        PolyxAsyncClass.destroy_event(async, timer);
        PolyxAsyncClass.free(async);
        return 1;
    }
    
    // Poll loop
    core->printf(core, "\nStarting poll loop...\n");
    for (int i = 0; i < 3; i++) {
        // Trigger custom event every other iteration
        if (i % 2 == 0) {
            core->printf(core, "Triggering custom event...\n");
            PolyxAsyncClass.trigger_event(async, event, (void*)event_data, core->strlen(core, event_data) + 1);
        }
        
        // Poll for events
        PolyxAsyncClass.poll(async, 1100);  // Slightly longer than timer interval
    }
    
    // Stop timer
    core->printf(core, "\nStopping timer...\n");
    PolyxAsyncClass.stop_timer(async, timer);
    
    // Cleanup
    PolyxAsyncClass.destroy_event(async, event);
    PolyxAsyncClass.destroy_event(async, timer);
    PolyxAsyncClass.free(async);
    
    core->printf(core, "\n=== All polyx_async tests completed ===\n");
    return 0;
}
