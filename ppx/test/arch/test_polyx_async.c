#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/polyx/PolyxAsync.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <time.h>//TODO use time function from our core


#include <unistd.h>

// Test context structures
typedef struct {
    int fd;             // File descriptor
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
    if (ctx->fd < 0) {
        log->debug(log, "async_read_file: opening file %s", ctx->filename);
        ctx->fd = open(ctx->filename, O_RDONLY | O_NONBLOCK);
        if (ctx->fd < 0) {
            log->debug(log, "async_read_file: failed to open file, errno=%d", errno);
            self->state = INFRAX_ASYNC_REJECTED;
            return;
        }
    }
    
    // Try to read
    ssize_t bytes = read(ctx->fd, 
                        ctx->buffer + ctx->bytes_processed,
                        ctx->size - ctx->bytes_processed);
    
    log->debug(log, "async_read_file: read returned %zd bytes", bytes);
    
    if (bytes > 0) {
        ctx->bytes_processed += bytes;
        log->debug(log, "async_read_file: total bytes read: %zu/%zu", 
               ctx->bytes_processed, ctx->size);
        if (ctx->bytes_processed < ctx->size) {
            ctx->yield_count++;
            log->debug(log, "async_read_file: yielding after successful read");
            InfraxAsyncClass.yield(self);
        } else {
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_FULFILLED;
        }
    } else if (bytes == 0) {
        // End of file
        log->debug(log, "async_read_file: reached EOF");
        close(ctx->fd);
        ctx->fd = -1;
        self->state = INFRAX_ASYNC_FULFILLED;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ctx->yield_count++;
            log->debug(log, "async_read_file: yielding on EAGAIN");
            InfraxAsyncClass.yield(self);
        } else {
            // Error occurred
            log->debug(log, "async_read_file: read error, errno=%d", errno);
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_REJECTED;
        }
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
    if (ctx->fd < 0) {
        log->debug(log, "async_write_file: opening file %s", ctx->filename);
        ctx->fd = open(ctx->filename, O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0644);
        if (ctx->fd < 0) {
            log->debug(log, "async_write_file: failed to open file, errno=%d", errno);
            self->state = INFRAX_ASYNC_REJECTED;
            return;
        }
    }
    
    // Try to write
    ssize_t bytes = write(ctx->fd, 
                         ctx->buffer + ctx->bytes_processed,
                         ctx->size - ctx->bytes_processed);
    
    log->debug(log, "async_write_file: write returned %zd bytes", bytes);
    
    if (bytes > 0) {
        ctx->bytes_processed += bytes;
        log->debug(log, "async_write_file: total bytes written: %zu/%zu", 
               ctx->bytes_processed, ctx->size);
        if (ctx->bytes_processed < ctx->size) {
            ctx->yield_count++;
            log->debug(log, "async_write_file: yielding after successful write");
            InfraxAsyncClass.yield(self);
        } else {
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_FULFILLED;
        }
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ctx->yield_count++;
            log->debug(log, "async_write_file: yielding on EAGAIN");
            InfraxAsyncClass.yield(self);
        } else {
            // Error occurred
            log->debug(log, "async_write_file: write error, errno=%d", errno);
            close(ctx->fd);
            ctx->fd = -1;
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
    
    // Create a test file
    FILE* fp = fopen(test_file, "w");
    INFRAX_ASSERT(core, fp != NULL);
    fprintf(fp, "Hello, World!");
    fclose(fp);
    log->debug(log, "test_polyx_async_read_file: test file created");
    
    // Prepare read context
    char buffer[128] = {0};
    AsyncFileContext ctx = {
        .fd = -1,
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
    
    // Wait for completion
    log->debug(log, "test_polyx_async_read_file: waiting for completion");
    static time_t last_status = 0;
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(async);
        }
        time_t now = time(NULL);
        if (now - last_status >= 1) {  // Log status every second
            log->debug(log, "test_polyx_async_read_file: waiting... (yield count: %d)", 
                        ctx.yield_count);
            last_status = now;
        }
        core->sleep_ms(core, 10);  // 10ms
    }
    
    log->info(log, "test_polyx_async_read_file: task completed");
    
    // Check result
    INFRAX_ASSERT(core, async->state == INFRAX_ASYNC_FULFILLED);
    INFRAX_ASSERT(core, strcmp("Hello, World!", buffer) == 0);
    
    // Cleanup
    log->debug(log, "test_polyx_async_read_file: cleaning up");
    InfraxAsyncClass.free(async);
    remove(test_file);
    log->debug(log, "test_polyx_async_read_file: cleanup complete");
}

void test_polyx_async_write_file(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "test_polyx_async_write_file: starting");
    
    const char* test_file = "test_write.txt";
    const char* test_data = "Hello, Write Test!";
    
    // Prepare write context
    AsyncFileContext ctx = {
        .fd = -1,
        .buffer = (char*)test_data,
        .size = strlen(test_data),
        .bytes_processed = 0,
        .filename = test_file,
        .yield_count = 0
    };
    
    // Create and start async task
    log->debug(log, "test_polyx_async_write_file: creating async task");
    InfraxAsync* async = InfraxAsyncClass.new(async_write_file, &ctx);
    INFRAX_ASSERT(core, async != NULL);
    
    log->debug(log, "test_polyx_async_write_file: starting async task");
    InfraxAsyncClass.start(async);
    
    // Wait for completion
    log->debug(log, "test_polyx_async_write_file: waiting for completion");
    static time_t last_status = 0;
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(async);
        }
        time_t now = time(NULL);
        if (now - last_status >= 1) {  // Log status every second
            log->debug(log, "test_polyx_async_write_file: waiting... (yield count: %d)", 
                        ctx.yield_count);
            last_status = now;
        }
        core->sleep_ms(core, 10);  // 10ms
    }
    
    log->info(log, "test_polyx_async_write_file: task completed");
    
    // Verify file contents
    FILE* fp = fopen(test_file, "r");
    INFRAX_ASSERT(core, fp != NULL);
    
    char buffer[100];
    size_t read = fread(buffer, 1, sizeof(buffer), fp);
    buffer[read] = '\0';
    fclose(fp);
    
    INFRAX_ASSERT(core, strcmp(test_data, buffer) == 0);
    
    // Cleanup
    log->debug(log, "test_polyx_async_write_file: cleaning up");
    InfraxAsyncClass.free(async);
    remove(test_file);
    log->debug(log, "test_polyx_async_write_file: cleanup complete");
}

// Async delay context
typedef struct {
    struct timespec start;
    int delay_ms;
} AsyncDelayContext;

// Async delay function
static void async_delay_func(InfraxAsync* self, void* arg) {
    AsyncDelayContext* ctx = (AsyncDelayContext*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    
    if (!ctx) {
        log->debug(log, "async_delay: ctx is NULL");
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    // Get current time and calculate elapsed
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    double elapsed = (current.tv_sec - ctx->start.tv_sec) + 
                    (current.tv_nsec - ctx->start.tv_nsec) / 1e9;
    double delay_sec = ctx->delay_ms / 1000.0;
    
    log->debug(log, "async_delay: elapsed=%.3f/%.3f seconds", 
                elapsed, delay_sec);
    
    // Check if delay is complete
    if (elapsed >= delay_sec) {
        log->debug(log, "async_delay: delay complete");
        self->state = INFRAX_ASYNC_FULFILLED;
        return;
    }
    
    // Yield until delay is complete
    InfraxAsyncClass.yield(self);
}

void test_polyx_async_delay(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    int delay_ms = 1000;
    
    log->info(log, "test_polyx_async_delay: starting (delay: %d ms)", delay_ms);
    
    // Create delay context
    AsyncDelayContext ctx;
    clock_gettime(CLOCK_MONOTONIC, &ctx.start);
    ctx.delay_ms = delay_ms;
    
    // Create async delay task
    InfraxAsync* async = InfraxAsyncClass.new(async_delay_func, &ctx);
    INFRAX_ASSERT(core, async != NULL);
    
    // Start the delay
    log->debug(log, "test_polyx_async_delay: starting async task");
    InfraxAsyncClass.start(async);
    
    // Wait for completion
    log->debug(log, "test_polyx_async_delay: waiting for completion");
    static time_t last_status = 0;
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(async);
        }
        time_t now = time(NULL);
        if (now - last_status >= 1) {  // Log status every second
            log->debug(log, "test_polyx_async_delay: waiting...");
            last_status = now;
        }
        core->sleep_ms(core, 10);  // 10ms
    }
    
    log->info(log, "test_polyx_async_delay: completed");
    
    // Cleanup
    InfraxAsyncClass.free(async);
}

void test_polyx_async_parallel(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "test_polyx_async_parallel: starting");
    
    // Create delay contexts
    AsyncDelayContext ctx1, ctx2, ctx3;
    clock_gettime(CLOCK_MONOTONIC, &ctx1.start);
    clock_gettime(CLOCK_MONOTONIC, &ctx2.start);
    clock_gettime(CLOCK_MONOTONIC, &ctx3.start);
    ctx1.delay_ms = 500;  // 500ms
    ctx2.delay_ms = 300;  // 300ms
    ctx3.delay_ms = 700;  // 700ms
    
    // Create multiple async tasks
    InfraxAsync* delay1 = InfraxAsyncClass.new(async_delay_func, &ctx1);
    InfraxAsync* delay2 = InfraxAsyncClass.new(async_delay_func, &ctx2);
    InfraxAsync* delay3 = InfraxAsyncClass.new(async_delay_func, &ctx3);
    
    INFRAX_ASSERT(core, delay1 != NULL);
    INFRAX_ASSERT(core, delay2 != NULL);
    INFRAX_ASSERT(core, delay3 != NULL);
    
    // Start all tasks
    log->debug(log, "test_polyx_async_parallel: starting all tasks");
    InfraxAsyncClass.start(delay1);
    InfraxAsyncClass.start(delay2);
    InfraxAsyncClass.start(delay3);
    
    // Wait for all tasks to complete
    log->debug(log, "test_polyx_async_parallel: waiting for completion");
    static time_t last_status = 0;
    while (!InfraxAsyncClass.is_done(delay1) || 
           !InfraxAsyncClass.is_done(delay2) || 
           !InfraxAsyncClass.is_done(delay3)) {
        if (delay1->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(delay1);
        }
        if (delay2->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(delay2);
        }
        if (delay3->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(delay3);
        }
        time_t now = time(NULL);
        if (now - last_status >= 1) {  // Log status every second
            log->debug(log, "test_polyx_async_parallel: waiting...");
            last_status = now;
        }
        core->sleep_ms(core, 10);  // 10ms
    }
    
    log->info(log, "test_polyx_async_parallel: all tasks completed");
    
    // Cleanup
    InfraxAsyncClass.free(delay1);
    InfraxAsyncClass.free(delay2);
    InfraxAsyncClass.free(delay3);
}

void test_polyx_async_sequence(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "test_polyx_async_sequence: starting");
    
    // Create delay contexts
    AsyncDelayContext ctx1, ctx2;
    clock_gettime(CLOCK_MONOTONIC, &ctx1.start);
    ctx1.delay_ms = 300;  // 300ms
    
    // Create and start first task
    InfraxAsync* delay1 = InfraxAsyncClass.new(async_delay_func, &ctx1);
    INFRAX_ASSERT(core, delay1 != NULL);
    
    log->debug(log, "test_polyx_async_sequence: starting first task");
    InfraxAsyncClass.start(delay1);
    
    // Wait for first task completion
    log->debug(log, "test_polyx_async_sequence: waiting for first task");
    static time_t last_status = 0;
    while (!InfraxAsyncClass.is_done(delay1)) {
        if (delay1->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(delay1);
        }
        time_t now = time(NULL);
        if (now - last_status >= 1) {  // Log status every second
            log->debug(log, "test_polyx_async_sequence: waiting for first task...");
            last_status = now;
        }
        core->sleep_ms(core, 10);
    }
    
    // Create and start second task
    clock_gettime(CLOCK_MONOTONIC, &ctx2.start);
    ctx2.delay_ms = 500;  // 500ms
    
    InfraxAsync* delay2 = InfraxAsyncClass.new(async_delay_func, &ctx2);
    INFRAX_ASSERT(core, delay2 != NULL);
    
    log->debug(log, "test_polyx_async_sequence: starting second task");
    InfraxAsyncClass.start(delay2);
    
    // Wait for second task completion
    log->debug(log, "test_polyx_async_sequence: waiting for second task");
    while (!InfraxAsyncClass.is_done(delay2)) {
        if (delay2->state == INFRAX_ASYNC_PENDING) {
            InfraxAsyncClass.start(delay2);
        }
        time_t now = time(NULL);
        if (now - last_status >= 1) {  // Log status every second
            log->debug(log, "test_polyx_async_sequence: waiting for second task...");
            last_status = now;
        }
        core->sleep_ms(core, 10);
    }
    
    log->info(log, "test_polyx_async_sequence: all tasks completed");
    
    // Cleanup
    InfraxAsyncClass.free(delay1);
    InfraxAsyncClass.free(delay2);
}

// Timer callback
static void test_timer_callback(void* arg) {
    int* count = (int*)arg;
    printf("Timer triggered, count: %d\n", *count);
    (*count)++;
}

// Event callback
static void test_event_callback(PolyxEvent* event, void* arg) {
    if (!event || !event->data) return;
    printf("Event triggered with data: %s\n", (char*)event->data);
}

int main() {
    printf("\n=== Testing PolyxAsync ===\n\n");
    
    // Create PolyxAsync instance
    PolyxAsync* async = PolyxAsyncClass.new();
    if (!async) {
        printf("Failed to create PolyxAsync instance\n");
        return 1;
    }
    
    // Test 1: Timer
    printf("Test 1: Timer\n");
    int timer_count = 0;
    
    PolyxTimerConfig timer_config = {
        .interval_ms = 1000,
        .is_periodic = true,
        .callback = test_timer_callback,
        .arg = &timer_count
    };
    
    PolyxEvent* timer = PolyxAsyncClass.create_timer(async, &timer_config);
    if (!timer) {
        printf("Failed to create timer\n");
        PolyxAsyncClass.free(async);
        return 1;
    }
    
    // Start timer
    printf("Starting timer...\n");
    PolyxAsyncClass.start_timer(async, timer);
    
    // Test 2: Custom Event
    printf("\nTest 2: Custom Event\n");
    
    const char* event_data = "Custom Event Data";
    PolyxEventConfig event_config = {
        .type = POLYX_EVENT_CUSTOM,
        .callback = test_event_callback,
        .arg = NULL,
        .data = (void*)event_data,
        .data_size = strlen(event_data) + 1
    };
    
    PolyxEvent* event = PolyxAsyncClass.create_event(async, &event_config);
    if (!event) {
        printf("Failed to create event\n");
        PolyxAsyncClass.destroy_event(async, timer);
        PolyxAsyncClass.free(async);
        return 1;
    }
    
    // Poll loop
    printf("\nStarting poll loop...\n");
    for (int i = 0; i < 3; i++) {
        // Trigger custom event every other iteration
        if (i % 2 == 0) {
            printf("Triggering custom event...\n");
            PolyxAsyncClass.trigger_event(async, event, (void*)event_data, strlen(event_data) + 1);
        }
        
        // Poll for events
        PolyxAsyncClass.poll(async, 1100);  // Slightly longer than timer interval
    }
    
    // Stop timer
    printf("\nStopping timer...\n");
    PolyxAsyncClass.stop_timer(async, timer);
    
    // Cleanup
    PolyxAsyncClass.destroy_event(async, event);
    PolyxAsyncClass.destroy_event(async, timer);
    PolyxAsyncClass.free(async);
    
    printf("\n=== All tests completed ===\n");
    return 0;
}
