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

// Test async read file function
static void async_read_file(InfraxAsync* self, void* arg) {
    AsyncFileContext* ctx = (AsyncFileContext*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    
    if (!ctx) {
        log->debug(log, "async_read_file: ctx is NULL");
        self->state = INFRAX_ASYNC_REJECTED;
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
    
    while (!InfraxAsyncClass.is_done(async)) {
        // Check timeout
        if (core->time_monotonic_ms(core) - start_time > TEST_TIMEOUT_MS) {
            log->error(log, "test_polyx_async_read_file: timeout after %d ms", TEST_TIMEOUT_MS);
            InfraxAsyncClass.cancel(async);
            break;
        }
        
        if (async->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(async);
        }
        InfraxAsyncClass.yield(async);
        InfraxTime now = core->time_monotonic_ms(core);
        if (now - last_status >= 1000) {  // Log status every second
            log->debug(log, "test_polyx_async_read_file: waiting... (yield count: %d)", 
                        ctx.yield_count);
            last_status = now;
        }
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
        async->poll(async, 100);  // 使用更短的轮询间隔
        
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
