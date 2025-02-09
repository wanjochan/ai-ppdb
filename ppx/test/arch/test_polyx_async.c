#include "internal/arch/PpxInfra.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/polyx/PolyxAsync.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
    
    if (!ctx) {
        core->printf(core, "[DEBUG] async_read_file: ctx is NULL\n");
        return;
    }
    
    // Open file if not already open
    if (ctx->fd < 0) {
        core->printf(core, "[DEBUG] async_read_file: opening file %s\n", ctx->filename);
        ctx->fd = open(ctx->filename, O_RDONLY | O_NONBLOCK);
        if (ctx->fd < 0) {
            core->printf(core, "[DEBUG] async_read_file: failed to open file, errno=%d\n", errno);
            self->state = INFRAX_ASYNC_REJECTED;
            return;
        }
    }
    
    // Try to read
    ssize_t bytes = read(ctx->fd, 
                        ctx->buffer + ctx->bytes_processed,
                        ctx->size - ctx->bytes_processed);
    
    core->printf(core, "[DEBUG] async_read_file: read returned %zd bytes\n", bytes);
    
    if (bytes > 0) {
        ctx->bytes_processed += bytes;
        core->printf(core, "[DEBUG] async_read_file: total bytes read: %zu/%zu\n", 
               ctx->bytes_processed, ctx->size);
        if (ctx->bytes_processed < ctx->size) {
            ctx->yield_count++;
            core->printf(core, "[DEBUG] async_read_file: yielding after successful read\n");
            self->yield(self);
        } else {
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_FULFILLED;
        }
    } else if (bytes == 0) {
        // End of file
        core->printf(core, "[DEBUG] async_read_file: reached EOF\n");
        close(ctx->fd);
        ctx->fd = -1;
        self->state = INFRAX_ASYNC_FULFILLED;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ctx->yield_count++;
            core->printf(core, "[DEBUG] async_read_file: yielding on EAGAIN\n");
            self->yield(self);
        } else {
            // Error occurred
            core->printf(core, "[DEBUG] async_read_file: read error, errno=%d\n", errno);
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
    
    if (!ctx) {
        core->printf(core, "[DEBUG] async_write_file: ctx is NULL\n");
        return;
    }
    
    // Open file if not already open
    if (ctx->fd < 0) {
        core->printf(core, "[DEBUG] async_write_file: opening file %s\n", ctx->filename);
        ctx->fd = open(ctx->filename, O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK, 0644);
        if (ctx->fd < 0) {
            core->printf(core, "[DEBUG] async_write_file: failed to open file, errno=%d\n", errno);
            self->state = INFRAX_ASYNC_REJECTED;
            return;
        }
    }
    
    // Try to write
    ssize_t bytes = write(ctx->fd, 
                         ctx->buffer + ctx->bytes_processed,
                         ctx->size - ctx->bytes_processed);
    
    core->printf(core, "[DEBUG] async_write_file: write returned %zd bytes\n", bytes);
    
    if (bytes > 0) {
        ctx->bytes_processed += bytes;
        core->printf(core, "[DEBUG] async_write_file: total bytes written: %zu/%zu\n", 
               ctx->bytes_processed, ctx->size);
        if (ctx->bytes_processed < ctx->size) {
            ctx->yield_count++;
            core->printf(core, "[DEBUG] async_write_file: yielding after successful write\n");
            self->yield(self);
        } else {
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_FULFILLED;
        }
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ctx->yield_count++;
            core->printf(core, "[DEBUG] async_write_file: yielding on EAGAIN\n");
            self->yield(self);
        } else {
            // Error occurred
            core->printf(core, "[DEBUG] async_write_file: write error, errno=%d\n", errno);
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_REJECTED;
        }
    }
}

// Test async file operations
void test_polyx_async_read_file(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "[DEBUG] test_polyx_async_read_file: starting\n");
    
    const char* test_file = "test.txt";
    
    // Create a test file
    FILE* fp = fopen(test_file, "w");
    INFRAX_ASSERT(core, fp != NULL);
    fprintf(fp, "Hello, World!");
    fclose(fp);
    core->printf(core, "[DEBUG] test_polyx_async_read_file: test file created\n");
    
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
    core->printf(core, "[DEBUG] test_polyx_async_read_file: creating async task\n");
    InfraxAsync* async = InfraxAsyncClass.new(async_read_file, &ctx);
    INFRAX_ASSERT(core, async != NULL);
    
    core->printf(core, "[DEBUG] test_polyx_async_read_file: starting async task\n");
    async->start(async, async_read_file, &ctx);
    
    // Wait for completion
    core->printf(core, "[DEBUG] test_polyx_async_read_file: waiting for completion\n");
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_read_file, &ctx);
        }
        core->sleep_ms(core, 10);  // 10ms
        core->printf(core, "[DEBUG] test_polyx_async_read_file: waiting... (yield count: %d)\n", 
                    ctx.yield_count);
    }
    
    core->printf(core, "[DEBUG] test_polyx_async_read_file: task completed\n");
    
    // Check result
    INFRAX_ASSERT(core, async->state == INFRAX_ASYNC_FULFILLED);
    INFRAX_ASSERT(core, strcmp("Hello, World!", buffer) == 0);
    
    // Cleanup
    core->printf(core, "[DEBUG] test_polyx_async_read_file: cleaning up\n");
    InfraxAsyncClass.free(async);
    remove(test_file);
    core->printf(core, "[DEBUG] test_polyx_async_read_file: cleanup complete\n");
}

void test_polyx_async_write_file(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "[DEBUG] test_polyx_async_write_file: starting\n");
    
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
    core->printf(core, "[DEBUG] test_polyx_async_write_file: creating async task\n");
    InfraxAsync* async = InfraxAsyncClass.new(async_write_file, &ctx);
    INFRAX_ASSERT(core, async != NULL);
    
    core->printf(core, "[DEBUG] test_polyx_async_write_file: starting async task\n");
    async->start(async, async_write_file, &ctx);
    
    // Wait for completion
    core->printf(core, "[DEBUG] test_polyx_async_write_file: waiting for completion\n");
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_write_file, &ctx);
        }
        core->sleep_ms(core, 10);  // 10ms
        core->printf(core, "[DEBUG] test_polyx_async_write_file: waiting... (yield count: %d)\n", 
                    ctx.yield_count);
    }
    
    core->printf(core, "[DEBUG] test_polyx_async_write_file: task completed\n");
    
    // Verify file contents
    FILE* fp = fopen(test_file, "r");
    INFRAX_ASSERT(core, fp != NULL);
    
    char buffer[100];
    size_t read = fread(buffer, 1, sizeof(buffer), fp);
    buffer[read] = '\0';
    fclose(fp);
    
    INFRAX_ASSERT(core, strcmp(test_data, buffer) == 0);
    
    // Cleanup
    core->printf(core, "[DEBUG] test_polyx_async_write_file: cleaning up\n");
    InfraxAsyncClass.free(async);
    remove(test_file);
    core->printf(core, "[DEBUG] test_polyx_async_write_file: cleanup complete\n");
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
    
    if (!ctx) {
        core->printf(core, "[DEBUG] async_delay: ctx is NULL\n");
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    // Get current time and calculate elapsed
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    double elapsed = (current.tv_sec - ctx->start.tv_sec) + 
                    (current.tv_nsec - ctx->start.tv_nsec) / 1e9;
    double delay_sec = ctx->delay_ms / 1000.0;
    
    core->printf(core, "[DEBUG] async_delay: elapsed=%.3f/%.3f seconds\n", 
                elapsed, delay_sec);
    
    // Check if delay is complete
    if (elapsed >= delay_sec) {
        core->printf(core, "[DEBUG] async_delay: delay complete\n");
        self->state = INFRAX_ASYNC_FULFILLED;
        return;
    }
    
    // Not done yet, yield and continue
    core->printf(core, "[DEBUG] async_delay: yielding\n");
    self->yield(self);
}

void test_polyx_async_delay(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    int delay_ms = 1000;
    
    core->printf(core, "[DEBUG] test_polyx_async_delay: starting (delay: %d ms)\n", delay_ms);
    
    // Create delay context
    AsyncDelayContext ctx;
    clock_gettime(CLOCK_MONOTONIC, &ctx.start);
    ctx.delay_ms = delay_ms;
    
    // Create async delay task
    InfraxAsync* async = InfraxAsyncClass.new(async_delay_func, &ctx);
    INFRAX_ASSERT(core, async != NULL);
    
    // Start the delay
    core->printf(core, "[DEBUG] test_polyx_async_delay: starting delay\n");
    async->start(async, async_delay_func, &ctx);
    
    // Wait for completion
    core->printf(core, "[DEBUG] test_polyx_async_delay: waiting for completion\n");
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_delay_func, &ctx);
        }
        core->sleep_ms(core, 10);  // 10ms
        core->printf(core, "[DEBUG] test_polyx_async_delay: waiting...\n");
    }
    
    core->printf(core, "[DEBUG] test_polyx_async_delay: completed\n");
    
    // Cleanup
    InfraxAsyncClass.free(async);
}

void test_polyx_async_parallel(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "[DEBUG] test_polyx_async_parallel: starting\n");
    
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
    core->printf(core, "[DEBUG] test_polyx_async_parallel: starting all tasks\n");
    delay1->start(delay1, async_delay_func, &ctx1);
    delay2->start(delay2, async_delay_func, &ctx2);
    delay3->start(delay3, async_delay_func, &ctx3);
    
    // Wait for all tasks to complete
    core->printf(core, "[DEBUG] test_polyx_async_parallel: waiting for completion\n");
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
        core->sleep_ms(core, 10);  // 10ms
        core->printf(core, "[DEBUG] test_polyx_async_parallel: waiting...\n");
    }
    
    core->printf(core, "[DEBUG] test_polyx_async_parallel: all tasks completed\n");
    
    // Cleanup
    InfraxAsyncClass.free(delay1);
    InfraxAsyncClass.free(delay2);
    InfraxAsyncClass.free(delay3);
}

void test_polyx_async_sequence(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->printf(core, "[DEBUG] test_polyx_async_sequence: starting\n");
    
    // Create delay contexts
    AsyncDelayContext ctx1, ctx2;
    clock_gettime(CLOCK_MONOTONIC, &ctx1.start);
    ctx1.delay_ms = 300;  // 300ms
    
    // Create and start first task
    InfraxAsync* delay1 = InfraxAsyncClass.new(async_delay_func, &ctx1);
    INFRAX_ASSERT(core, delay1 != NULL);
    
    core->printf(core, "[DEBUG] test_polyx_async_sequence: starting first task\n");
    delay1->start(delay1, async_delay_func, &ctx1);
    
    while (delay1->state != INFRAX_ASYNC_FULFILLED) {
        if (delay1->state == INFRAX_ASYNC_PENDING) {
            delay1->start(delay1, async_delay_func, &ctx1);
        }
        core->sleep_ms(core, 10);
        core->printf(core, "[DEBUG] test_polyx_async_sequence: waiting for first task...\n");
    }
    
    // Create and start second task
    clock_gettime(CLOCK_MONOTONIC, &ctx2.start);
    ctx2.delay_ms = 500;  // 500ms
    
    InfraxAsync* delay2 = InfraxAsyncClass.new(async_delay_func, &ctx2);
    INFRAX_ASSERT(core, delay2 != NULL);
    
    core->printf(core, "[DEBUG] test_polyx_async_sequence: starting second task\n");
    delay2->start(delay2, async_delay_func, &ctx2);
    
    while (delay2->state != INFRAX_ASYNC_FULFILLED) {
        if (delay2->state == INFRAX_ASYNC_PENDING) {
            delay2->start(delay2, async_delay_func, &ctx2);
        }
        core->sleep_ms(core, 10);
        core->printf(core, "[DEBUG] test_polyx_async_sequence: waiting for second task...\n");
    }
    
    core->printf(core, "[DEBUG] test_polyx_async_sequence: all tasks completed\n");
    
    // Cleanup
    InfraxAsyncClass.free(delay1);
    InfraxAsyncClass.free(delay2);
}

int main(void) {
    printf("===================\nStarting PolyxAsync tests...\n");

    test_polyx_async_read_file();
    test_polyx_async_write_file();
    test_polyx_async_delay();
    test_polyx_async_parallel();
    test_polyx_async_sequence();
    printf("All tests passed!\n");
    return 0;
}
