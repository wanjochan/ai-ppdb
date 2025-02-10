#include "internal/infrax/InfraxCore.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxLog.h"

// Forward declarations
void infrax_scheduler_init(void);
void infrax_scheduler_poll(void);

// Test configuration
#define DELAY_SECONDS 1.0

// Test context structure
typedef struct {
    int fd;             // File descriptor
    char* buffer;       // Read buffer
    size_t size;        // Buffer size
    size_t bytes_read;  // Total bytes read
    const char* filename;
    int yield_count;    // Count how many times yield is called
} AsyncReadContext;

// Context for async delay operations
typedef struct {
    double delay_seconds;  // Delay duration in seconds
    InfraxTime start_time;    // Start time of the delay in ms
    InfraxTime end_time;      // End time of the delay in ms
} AsyncDelayContext;

// Async read file function
static void async_read_file(InfraxAsync* self, void* arg) {
    AsyncReadContext* ctx = (AsyncReadContext*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    
    if (!ctx) {
        log->error(log, "async_read_file: ctx is NULL");
        return;
    }
    
    // Open file if not already open
    if (ctx->fd < 0) {
        log->debug(log, "async_read_file: opening file %s", ctx->filename);
        ctx->fd = open(ctx->filename, O_RDONLY | O_NONBLOCK);
        if (ctx->fd < 0) {
            log->error(log, "async_read_file: failed to open file, errno=%d", errno);
            self->state = INFRAX_ASYNC_REJECTED;
            self->error = errno;
            return;
        }
        
        // Yield after file open
        ctx->yield_count++;
        InfraxAsyncClass.yield(self);
    }
    
    // Read file in chunks
    while (ctx->bytes_read < ctx->size) {
        ssize_t n = read(ctx->fd, ctx->buffer + ctx->bytes_read, ctx->size - ctx->bytes_read);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Yield on non-blocking read
                ctx->yield_count++;
                InfraxAsyncClass.yield(self);
                continue;
            }
            // Error occurred
            log->error(log, "async_read_file: read error, errno=%d", errno);
            self->state = INFRAX_ASYNC_REJECTED;
            self->error = errno;
            return;
        }
        if (n == 0) {
            // EOF reached
            break;
        }
        ctx->bytes_read += n;
    }
    
    // Store result
    InfraxAsyncClass.set_result(self, ctx->buffer, ctx->bytes_read);
    self->state = INFRAX_ASYNC_FULFILLED;
    
    // Close file
    close(ctx->fd);
    ctx->fd = -1;
}

// Test async file operations
void test_async_file_read(void) {
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "Testing async file read...");
    
    // Setup test context
    AsyncReadContext ctx = {
        .fd = -1,
        .buffer = malloc(1024),
        .size = 1024,
        .bytes_read = 0,
        .filename = "/etc/hosts",
        .yield_count = 0
    };
    
    // Create and start async task
    InfraxAsync* async = InfraxAsyncClass.new(async_read_file, &ctx);
    if (!async) {
        log->error(log, "Failed to create async task");
        return;
    }
    
    // Start task
    InfraxAsyncClass.start(async);
    
    // Process until done
    while (async->state == INFRAX_ASYNC_PENDING) {
        // Poll scheduler to process tasks
        infrax_scheduler_poll();
    }
    
    // Check result
    if (async->state == INFRAX_ASYNC_FULFILLED) {
        size_t size;
        void* data = InfraxAsyncClass.get_result(async, &size);
        log->info(log, "Read %zu bytes from file", size);
        log->info(log, "Yielded %d times", ctx.yield_count);
    } else {
        log->error(log, "File read failed with error: %d", async->error);
    }
    
    // Cleanup
    free(ctx.buffer);
    InfraxAsyncClass.free(async);
}

// Async delay function
void async_delay(InfraxAsync* self, void* arg) {
    AsyncDelayContext* ctx = (AsyncDelayContext*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    
    if (!ctx) return;
    
    // Record start time
    ctx->start_time = core->time_now_ms(core);
    
    // Yield until enough time has passed
    while (1) {
        ctx->end_time = core->time_now_ms(core);
        double elapsed = (ctx->end_time - ctx->start_time) / 1000.0;
        if (elapsed >= ctx->delay_seconds) break;
        InfraxAsyncClass.yield(self);
    }
    
    // Store result
    double elapsed = (ctx->end_time - ctx->start_time) / 1000.0;
    InfraxAsyncClass.set_result(self, &elapsed, sizeof(elapsed));
    self->state = INFRAX_ASYNC_FULFILLED;
}

// Test async delay
void test_async_delay(void) {
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "Testing async delay...");
    
    // Setup delay context
    AsyncDelayContext delay_ctx = {
        .delay_seconds = DELAY_SECONDS
    };
    
    // Create and start async task
    InfraxAsync* async = InfraxAsyncClass.new(async_delay, &delay_ctx);
    if (!async) {
        log->error(log, "Failed to create async task");
        return;
    }
    
    // Start task
    InfraxAsyncClass.start(async);
    
    // Process until done
    while (async->state == INFRAX_ASYNC_PENDING) {
        // Poll scheduler to process tasks
        infrax_scheduler_poll();
    }
    
    // Check result
    if (async->state == INFRAX_ASYNC_FULFILLED) {
        size_t size;
        double* elapsed = InfraxAsyncClass.get_result(async, &size);
        if (elapsed && size == sizeof(double)) {
            log->info(log, "Delay completed in %.3f seconds", *elapsed);
        }
    } else {
        log->error(log, "Delay failed with error: %d", async->error);
    }
    
    // Cleanup
    InfraxAsyncClass.free(async);
}

// Test concurrent async operations
void test_async_concurrent(void) {
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "Testing concurrent async operations...");
    
    // Setup read context
    AsyncReadContext ctx = {
        .fd = -1,
        .buffer = malloc(1024),
        .size = 1024,
        .bytes_read = 0,
        .filename = "/etc/hosts",
        .yield_count = 0
    };
    
    // Setup delay context
    AsyncDelayContext delay_ctx = {
        .delay_seconds = DELAY_SECONDS
    };
    
    // Create tasks
    InfraxAsync* read_task = InfraxAsyncClass.new(async_read_file, &ctx);
    InfraxAsync* delay_task = InfraxAsyncClass.new(async_delay, &delay_ctx);
    
    if (!read_task || !delay_task) {
        log->error(log, "Failed to create async tasks");
        if (read_task) InfraxAsyncClass.free(read_task);
        if (delay_task) InfraxAsyncClass.free(delay_task);
        free(ctx.buffer);
        return;
    }
    
    // Start both tasks
    InfraxAsyncClass.start(read_task);
    InfraxAsyncClass.start(delay_task);
    
    // Process until both are done
    while (read_task->state == INFRAX_ASYNC_PENDING || 
           delay_task->state == INFRAX_ASYNC_PENDING) {
        // Poll scheduler to process tasks
        infrax_scheduler_poll();
    }
    
    // Check results
    if (read_task->state == INFRAX_ASYNC_FULFILLED) {
        size_t size;
        void* data = InfraxAsyncClass.get_result(read_task, &size);
        log->info(log, "Read task completed: %zu bytes", size);
    }
    
    if (delay_task->state == INFRAX_ASYNC_FULFILLED) {
        size_t size;
        double* elapsed = InfraxAsyncClass.get_result(delay_task, &size);
        if (elapsed && size == sizeof(double)) {
            log->info(log, "Delay task completed in %.3f seconds", *elapsed);
        }
    }
    
    // Cleanup
    free(ctx.buffer);
    InfraxAsyncClass.free(read_task);
    InfraxAsyncClass.free(delay_task);
}

// Main test function
int main(void) {
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "Starting InfraxAsync tests...");
    
    // Initialize scheduler
    infrax_scheduler_init();
    
    test_async_file_read();
    test_async_delay();
    test_async_concurrent();
    
    log->info(log, "All InfraxAsync tests completed!");
    return 0;
}
