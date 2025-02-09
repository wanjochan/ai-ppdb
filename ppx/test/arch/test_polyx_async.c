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
            self->yield(self);
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
            self->yield(self);
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
            self->yield(self);
        } else {
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_FULFILLED;
        }
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ctx->yield_count++;
            log->debug(log, "async_write_file: yielding on EAGAIN");
            self->yield(self);
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
    async->start(async, async_read_file, &ctx);
    
    // Wait for completion
    log->debug(log, "test_polyx_async_read_file: waiting for completion");
    static time_t last_status = 0;
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_read_file, &ctx);
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
    async->start(async, async_write_file, &ctx);
    
    // Wait for completion
    log->debug(log, "test_polyx_async_write_file: waiting for completion");
    static time_t last_status = 0;
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_write_file, &ctx);
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
    
    // Not done yet, yield and continue
    log->debug(log, "async_delay: yielding");
    self->yield(self);
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
    log->debug(log, "test_polyx_async_delay: starting delay");
    async->start(async, async_delay_func, &ctx);
    
    // Wait for completion
    log->debug(log, "test_polyx_async_delay: waiting for completion");
    static time_t last_status = 0;
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_delay_func, &ctx);
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
    delay1->start(delay1, async_delay_func, &ctx1);
    delay2->start(delay2, async_delay_func, &ctx2);
    delay3->start(delay3, async_delay_func, &ctx3);
    
    // Wait for all tasks to complete
    log->debug(log, "test_polyx_async_parallel: waiting for completion");
    static time_t last_status = 0;
    while (delay1->state != INFRAX_ASYNC_FULFILLED || 
           delay2->state != INFRAX_ASYNC_FULFILLED || 
           delay3->state != INFRAX_ASYNC_FULFILLED) {
        if (delay1->state == INFRAX_ASYNC_PENDING) {
            delay1->start(delay1, async_delay_func, &ctx1);
        }
        if (delay2->state == INFRAX_ASYNC_PENDING) {
            delay2->start(delay2, async_delay_func, &ctx2);
        }
        if (delay3->state == INFRAX_ASYNC_PENDING) {
            delay3->start(delay3, async_delay_func, &ctx3);
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
    delay1->start(delay1, async_delay_func, &ctx1);
    
    static time_t last_status1 = 0;
    while (delay1->state != INFRAX_ASYNC_FULFILLED) {
        if (delay1->state == INFRAX_ASYNC_PENDING) {
            delay1->start(delay1, async_delay_func, &ctx1);
        }
        time_t now = time(NULL);
        if (now - last_status1 >= 1) {  // Log status every second
            log->debug(log, "test_polyx_async_sequence: waiting for first task...");
            last_status1 = now;
        }
        core->sleep_ms(core, 10);
    }
    
    // Create and start second task
    clock_gettime(CLOCK_MONOTONIC, &ctx2.start);
    ctx2.delay_ms = 500;  // 500ms
    
    InfraxAsync* delay2 = InfraxAsyncClass.new(async_delay_func, &ctx2);
    INFRAX_ASSERT(core, delay2 != NULL);
    
    log->debug(log, "test_polyx_async_sequence: starting second task");
    delay2->start(delay2, async_delay_func, &ctx2);
    
    static time_t last_status2 = 0;
    while (delay2->state != INFRAX_ASYNC_FULFILLED) {
        if (delay2->state == INFRAX_ASYNC_PENDING) {
            delay2->start(delay2, async_delay_func, &ctx2);
        }
        time_t now = time(NULL);
        if (now - last_status2 >= 1) {  // Log status every second
            log->debug(log, "test_polyx_async_sequence: waiting for second task...");
            last_status2 = now;
        }
        core->sleep_ms(core, 10);
    }
    
    log->info(log, "test_polyx_async_sequence: all tasks completed");
    
    // Cleanup
    InfraxAsyncClass.free(delay1);
    InfraxAsyncClass.free(delay2);
}

int main(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    
    log->info(log, "===================\nStarting PolyxAsync tests...");
    
    test_polyx_async_read_file();
    test_polyx_async_write_file();
    test_polyx_async_delay();
    test_polyx_async_parallel();
    test_polyx_async_sequence();
    
    log->info(log, "All tests passed!\n===================");
    return 0;
}
