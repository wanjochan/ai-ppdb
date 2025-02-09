// #include <assert.h> use assert from our core
#include "internal/infrax/InfraxCore.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <time.h> //TODO use time function from our core
#include <unistd.h>
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxLog.h"

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
            return;
        }
    }
    
    // Try to read
    ssize_t bytes = read(ctx->fd, 
                        ctx->buffer + ctx->bytes_read,
                        ctx->size - ctx->bytes_read);
    
    log->debug(log, "async_read_file: read returned %zd bytes", bytes);
    
    if (bytes > 0) {
        ctx->bytes_read += bytes;
        log->debug(log, "async_read_file: total bytes read: %zu/%zu", 
               ctx->bytes_read, ctx->size);
        if (ctx->bytes_read < ctx->size) {
            ctx->yield_count++;
            log->debug(log, "async_read_file: yielding after successful read");
            self->yield(self);
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
            log->error(log, "async_read_file: read error, errno=%d", errno);
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_REJECTED;
        }
    }
}

// Test async file operations
static void test_async_file_read(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->debug(log, "test_async_file_read: starting");
    
    // Create a test file
    FILE* f = fopen("test_async.txt", "w");
    INFRAX_ASSERT(core, f != NULL);
    fputs("Hello, Async World!", f);
    fclose(f);
    log->debug(log, "test_async_file_read: created test file");
    
    // Prepare read context
    char buffer[128] = {0};
    AsyncReadContext ctx = {
        .fd = -1,
        .buffer = buffer,
        .size = sizeof(buffer),
        .bytes_read = 0,
        .filename = "test_async.txt",
        .yield_count = 0
    };
    
    // Create and start async task
    InfraxAsync* async = InfraxAsyncClass.new(async_read_file, &ctx);
    INFRAX_ASSERT(core, async != NULL);
    log->debug(log, "test_async_file_read: created async task");
    
    // Start the task
    async->start(async, async_read_file, &ctx);
    log->debug(log, "test_async_file_read: started async task");
    
    // Run until complete
    while (async->state != INFRAX_ASYNC_FULFILLED) {
        log->debug(log, "test_async_file_read: task status: %d", async->state);
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_read_file, &ctx);
        }
        usleep(1000);  // 1ms sleep
    }
    
    // Verify content
    INFRAX_ASSERT(core, strncmp(buffer, "Hello, Async World!", strlen("Hello, Async World!")) == 0);
    // Verify that yield was called at least once
    INFRAX_ASSERT(core, ctx.yield_count > 0);
    log->debug(log, "test_async_file_read: content matches, yielded %d times", ctx.yield_count);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    unlink("test_async.txt");
    log->debug(log, "test_async_file_read: cleanup complete");
}

// Async delay function
static void async_delay(InfraxAsync* self, void* arg) {
    AsyncDelayContext* ctx = (AsyncDelayContext*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    
    if (!ctx) {
        log->error(log, "async_delay: ctx is NULL");
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    // Initialize start time if not set
    if (ctx->start_time == 0) {
        ctx->start_time = core->time_monotonic_ms(core);
        log->debug(log, "async_delay: initializing start time");
    }
    
    // Calculate elapsed time
    InfraxTime current = core->time_monotonic_ms(core);
    double elapsed = (current - ctx->start_time) / 1000.0;  // Convert to seconds
    
    // Only log every 0.1 seconds or at specific points
    static double last_log = 0.0;
    if (elapsed - last_log >= 0.1 || elapsed >= ctx->delay_seconds) {
        log->debug(log, "async_delay: elapsed=%.3f/%.3f seconds", 
               elapsed, ctx->delay_seconds);
        last_log = elapsed;
    }
    
    if (elapsed >= ctx->delay_seconds) {
        log->debug(log, "async_delay: delay complete");
        ctx->end_time = current;
        self->state = INFRAX_ASYNC_FULFILLED;
        return;
    }
    
    // Yield to allow other tasks to run
    self->yield(self);
}

// Test async delay
static void test_async_delay(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "Starting delay test (will wait for %.3f seconds)...", DELAY_SECONDS);
    
    InfraxTime start = core->time_monotonic_ms(core);
    
    // Create and start async delay task
    AsyncDelayContext delay_ctx = {
        .delay_seconds = DELAY_SECONDS,
        .start_time = 0,
        .end_time = 0
    };
    InfraxAsync* async = InfraxAsyncClass.new(async_delay, &delay_ctx);
    INFRAX_ASSERT(core, async != NULL);
    
    // Start the task
    async->start(async, async_delay, &delay_ctx);
    
    // Run task until completion
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        
        log->debug(log, "test_async_delay: task status: %d", async->state);
        
        // Resume task if yielded
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_delay, &delay_ctx);
        }
        
        core->sleep_ms(core, 1);  // 1ms sleep
    }
    
    // Check for error
    if (async->state == INFRAX_ASYNC_REJECTED) {
        log->error(log, "Async delay test failed: task returned error");
        InfraxAsyncClass.free(async);
        INFRAX_ASSERT(core, 0);  // Force test failure
    }
    
    InfraxTime current = core->time_monotonic_ms(core);
    double elapsed = (current - start) / 1000.0;  // Convert to seconds
    
    // Verify that approximately DELAY_SECONDS has passed
    INFRAX_ASSERT(core, elapsed >= DELAY_SECONDS);
    INFRAX_ASSERT(core, elapsed <= DELAY_SECONDS + 0.1);  // Allow 100ms margin
    log->info(log, "Async delay test passed: waited for %.3f seconds", elapsed);
    
    // Cleanup
    InfraxAsyncClass.free(async);
}

// Test concurrent async operations
static void test_async_concurrent(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->debug(log, "test_async_concurrent: starting");
    
    // Prepare read context
    char buffer[128] = {0};
    AsyncReadContext ctx = {
        .fd = -1,
        .buffer = buffer,
        .size = sizeof(buffer),
        .bytes_read = 0,
        .filename = "test_async.txt",
        .yield_count = 0
    };
    
    // Create test file
    FILE* f = fopen("test_async.txt", "w");
    INFRAX_ASSERT(core, f != NULL);
    fputs("Hello, Async World!", f);
    fclose(f);
    log->debug(log, "test_async_concurrent: created test file");
    
    // Record start time
    time_t start_time = time(NULL);
    log->debug(log, "test_async_concurrent: start time recorded");
    
    // Create async tasks
    InfraxAsync* read_task = InfraxAsyncClass.new(async_read_file, &ctx);
    AsyncDelayContext delay_ctx = {
        .delay_seconds = DELAY_SECONDS,
        .start_time = 0,
        .end_time = 0
    };
    InfraxAsync* delay_task = InfraxAsyncClass.new(async_delay, &delay_ctx);
    INFRAX_ASSERT(core, read_task != NULL && delay_task != NULL);
    log->debug(log, "test_async_concurrent: tasks created");
    
    // Start both tasks
    read_task->start(read_task, async_read_file, &ctx);
    delay_task->start(delay_task, async_delay, &delay_ctx);
    log->debug(log, "test_async_concurrent: tasks started");
    
    // Run until both complete
    while (read_task->state != INFRAX_ASYNC_FULFILLED ||
           delay_task->state != INFRAX_ASYNC_FULFILLED) {
        
        log->debug(log, "test_async_concurrent: read_task state=%d, delay_task state=%d",
               read_task->state, delay_task->state);
        
        // Resume read task if yielded
        if (read_task->state == INFRAX_ASYNC_PENDING) {
            log->debug(log, "test_async_concurrent: resuming read task");
            read_task->start(read_task, async_read_file, &ctx);
        }
        
        // Resume delay task if yielded
        if (delay_task->state == INFRAX_ASYNC_PENDING) {
            log->debug(log, "test_async_concurrent: resuming delay task");
            delay_task->start(delay_task, async_delay, &delay_ctx);
        }
        
        usleep(1000);  // Small delay
    }
    
    // Record end time
    time_t end_time = time(NULL);
    log->debug(log, "test_async_concurrent: tasks completed");
    
    // Verify results
    INFRAX_ASSERT(core, strncmp(buffer, "Hello, Async World!", strlen("Hello, Async World!")) == 0);
    INFRAX_ASSERT(core, end_time - start_time >= DELAY_SECONDS);
    
    log->debug(log, "test_async_concurrent: verification passed");
    log->info(log, "Concurrent test completed! Total time: %ld seconds", end_time - start_time);
    
    // Cleanup
    InfraxAsyncClass.free(read_task);
    InfraxAsyncClass.free(delay_task);
    unlink("test_async.txt");
    log->debug(log, "test_async_concurrent: cleanup complete");
}

void test_async_io() {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "Testing async I/O...");
    
    // Create a pipe for testing
    int pipefd[2];
    int ret = pipe(pipefd);
    INFRAX_ASSERT(core, ret == 0);
    
    // Set non-blocking mode
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    
    // Write some data
    const char* test_data = "Hello, Async!";
    ssize_t written = write(pipefd[1], test_data, strlen(test_data));
    INFRAX_ASSERT(core, written == strlen(test_data));
    
    // Read the data
    char buffer[128];
    ssize_t nread = read(pipefd[0], buffer, sizeof(buffer));
    INFRAX_ASSERT(core, nread == strlen(test_data));
    buffer[nread] = '\0';
    INFRAX_ASSERT(core, strcmp(buffer, test_data) == 0);
    
    // Clean up
    close(pipefd[0]);
    close(pipefd[1]);
    
    log->info(log, "Async I/O test passed");
}

void test_async_events() {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "Testing async events...");
    
    // Create event sources
    int event_pipe[2];
    int ret = pipe(event_pipe);
    INFRAX_ASSERT(core, ret == 0);
    
    // Set non-blocking mode
    fcntl(event_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(event_pipe[1], F_SETFL, O_NONBLOCK);
    
    // Trigger an event
    const char event_data = 1;
    ssize_t written = write(event_pipe[1], &event_data, 1);
    INFRAX_ASSERT(core, written == 1);
    
    // Read the event
    char buffer;
    ssize_t nread = read(event_pipe[0], &buffer, 1);
    INFRAX_ASSERT(core, nread == 1);
    INFRAX_ASSERT(core, buffer == event_data);
    
    // Clean up
    close(event_pipe[0]);
    close(event_pipe[1]);
    
    log->info(log, "Async events test passed");
}

int main(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "===================\nStarting InfraxAsync tests...");
    
    test_async_file_read();
    test_async_delay();
    test_async_concurrent();
    test_async_io();
    test_async_events();
    
    log->info(log, "All tests passed!\n===================");
    return 0;
}
