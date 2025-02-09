// #include <assert.h> use assert from our core
#include "internal/infrax/InfraxCore.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "internal/infrax/InfraxAsync.h"

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

// Async read file function
static void async_read_file(InfraxAsync* self, void* arg) {
    AsyncReadContext* ctx = (AsyncReadContext*)arg;
    if (!ctx) {
        printf("[DEBUG] async_read_file: ctx is NULL\n");
        return;
    }
    
    // Open file if not already open
    if (ctx->fd < 0) {
        printf("[DEBUG] async_read_file: opening file %s\n", ctx->filename);
        ctx->fd = open(ctx->filename, O_RDONLY | O_NONBLOCK);
        if (ctx->fd < 0) {
            printf("[DEBUG] async_read_file: failed to open file, errno=%d\n", errno);
            self->state = INFRAX_ASYNC_REJECTED;
            return;
        }
    }
    
    // Try to read
    ssize_t bytes = read(ctx->fd, 
                        ctx->buffer + ctx->bytes_read,
                        ctx->size - ctx->bytes_read);
    
    printf("[DEBUG] async_read_file: read returned %zd bytes\n", bytes);
    
    if (bytes > 0) {
        ctx->bytes_read += bytes;
        printf("[DEBUG] async_read_file: total bytes read: %zu/%zu\n", 
               ctx->bytes_read, ctx->size);
        if (ctx->bytes_read < ctx->size) {
            ctx->yield_count++;
            printf("[DEBUG] async_read_file: yielding after successful read\n");
            self->yield(self);
        }
    } else if (bytes == 0) {
        // End of file
        printf("[DEBUG] async_read_file: reached EOF\n");
        close(ctx->fd);
        ctx->fd = -1;
        self->state = INFRAX_ASYNC_FULFILLED;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            ctx->yield_count++;
            printf("[DEBUG] async_read_file: yielding on EAGAIN\n");
            self->yield(self);
        } else {
            // Error occurred
            printf("[DEBUG] async_read_file: read error, errno=%d\n", errno);
            close(ctx->fd);
            ctx->fd = -1;
            self->state = INFRAX_ASYNC_REJECTED;
        }
    }
}

// Test async file operations
static void test_async_file_read(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    printf("[DEBUG] test_async_file_read: starting\n");
    
    // Create a test file
    FILE* f = fopen("test_async.txt", "w");
    INFRAX_ASSERT(core, f != NULL);
    fputs("Hello, Async World!", f);
    fclose(f);
    printf("[DEBUG] test_async_file_read: created test file\n");
    
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
    printf("[DEBUG] test_async_file_read: created async task\n");
    
    // Start the task
    async->start(async, async_read_file, &ctx);
    printf("[DEBUG] test_async_file_read: started async task\n");
    
    // Run until complete
    while (async->state != INFRAX_ASYNC_FULFILLED) {
        printf("[DEBUG] test_async_file_read: task status: %d\n", async->state);
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_read_file, &ctx);
        }
        usleep(1000);  // 1ms sleep
    }
    
    // Verify content
    INFRAX_ASSERT(core, strncmp(buffer, "Hello, Async World!", strlen("Hello, Async World!")) == 0);
    // Verify that yield was called at least once
    INFRAX_ASSERT(core, ctx.yield_count > 0);
    printf("[DEBUG] test_async_file_read: content matches, yielded %d times\n", ctx.yield_count);
    
    // Cleanup
    InfraxAsyncClass.free(async);
    unlink("test_async.txt");
    printf("[DEBUG] test_async_file_read: cleanup complete\n");
}

// Async delay function
static void async_delay(InfraxAsync* self, void* arg) {
    // Store start time in user_data if first time
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    if (!ctx->user_data) {
        printf("[DEBUG] async_delay: initializing start time\n");
        struct timespec* start = malloc(sizeof(struct timespec));
        if (!start) {
            printf("[DEBUG] async_delay: failed to allocate memory\n");
            self->state = INFRAX_ASYNC_REJECTED;
            return;
        }
        clock_gettime(CLOCK_MONOTONIC, start);
        ctx->user_data = start;
    }
    
    // Get current time and calculate elapsed
    //TODO use core->time_now_ms() instead of clock_gettime
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    struct timespec* start = (struct timespec*)ctx->user_data;
    double elapsed = (current.tv_sec - start->tv_sec) + 
                    (current.tv_nsec - start->tv_nsec) / 1e9;
    
    printf("[DEBUG] async_delay: elapsed=%.3f seconds\n", elapsed);
    
    // Check if delay is complete
    if (elapsed >= DELAY_SECONDS) {
        printf("[DEBUG] async_delay: delay complete\n");
        free(start);
        ctx->user_data = NULL;
        self->state = INFRAX_ASYNC_FULFILLED;
        return;  // Delay complete
    }
    
    // Not done yet, yield and continue
    printf("[DEBUG] async_delay: yielding\n");
    self->yield(self);
}

// Test async delay
static void test_async_delay(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    printf("Starting delay test (will wait for %.3f seconds)...\n", DELAY_SECONDS);
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Create and start async delay task
    InfraxAsync* async = InfraxAsyncClass.new(async_delay, NULL);
    INFRAX_ASSERT(core, async != NULL);
    
    // Start the task
    async->start(async, async_delay, NULL);
    
    // Run task until completion
    while (async->state != INFRAX_ASYNC_FULFILLED && 
           async->state != INFRAX_ASYNC_REJECTED) {
        if (async->state == INFRAX_ASYNC_PENDING) {
            async->start(async, async_delay, NULL);
        }
        usleep(1000);  // 1ms sleep
    }
    
    // Check for error
    if (async->state == INFRAX_ASYNC_REJECTED) {
        printf("Async delay test failed: task returned error\n");
        InfraxAsyncClass.free(async);
        INFRAX_ASSERT(core, 0);  // Force test failure
    }
    
    clock_gettime(CLOCK_MONOTONIC, &current);
    double elapsed = (current.tv_sec - start.tv_sec) + 
                    (current.tv_nsec - start.tv_nsec) / 1e9;
    
    // Verify that approximately DELAY_SECONDS has passed
    INFRAX_ASSERT(core, elapsed >= DELAY_SECONDS);
    INFRAX_ASSERT(core, elapsed <= DELAY_SECONDS + 0.1);  // Allow 100ms margin
    printf("Async delay test passed: waited for %.3f seconds\n", elapsed);
    
    // Cleanup
    InfraxAsyncClass.free(async);
}

// Test concurrent async operations
static void test_async_concurrent(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    printf("[DEBUG] test_async_concurrent: starting\n");
    
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
    printf("[DEBUG] test_async_concurrent: created test file\n");
    
    // Record start time
    time_t start_time = time(NULL);
    printf("[DEBUG] test_async_concurrent: start time recorded\n");
    
    // Create async tasks
    InfraxAsync* read_task = InfraxAsyncClass.new(async_read_file, &ctx);
    InfraxAsync* delay_task = InfraxAsyncClass.new(async_delay, NULL);
    INFRAX_ASSERT(core, read_task != NULL && delay_task != NULL);
    printf("[DEBUG] test_async_concurrent: tasks created\n");
    
    // Start both tasks
    read_task->start(read_task, async_read_file, &ctx);
    delay_task->start(delay_task, async_delay, NULL);
    printf("[DEBUG] test_async_concurrent: tasks started\n");
    
    // Run until both complete
    while (read_task->state != INFRAX_ASYNC_FULFILLED ||
           delay_task->state != INFRAX_ASYNC_FULFILLED) {
        
        printf("[DEBUG] test_async_concurrent: read_task state=%d, delay_task state=%d\n",
               read_task->state, delay_task->state);
        
        // Resume read task if yielded
        if (read_task->state == INFRAX_ASYNC_PENDING) {
            printf("[DEBUG] test_async_concurrent: resuming read task\n");
            read_task->start(read_task, async_read_file, &ctx);
        }
        
        // Resume delay task if yielded
        if (delay_task->state == INFRAX_ASYNC_PENDING) {
            printf("[DEBUG] test_async_concurrent: resuming delay task\n");
            delay_task->start(delay_task, async_delay, NULL);
        }
        
        usleep(1000);  // Small delay
    }
    
    // Record end time
    time_t end_time = time(NULL);
    printf("[DEBUG] test_async_concurrent: tasks completed\n");
    
    // Verify results
    INFRAX_ASSERT(core, strncmp(buffer, "Hello, Async World!", strlen("Hello, Async World!")) == 0);
    INFRAX_ASSERT(core, end_time - start_time >= DELAY_SECONDS);
    
    printf("[DEBUG] test_async_concurrent: verification passed\n");
    printf("Concurrent test completed! Total time: %ld seconds\n", end_time - start_time);
    
    // Cleanup
    InfraxAsyncClass.free(read_task);
    InfraxAsyncClass.free(delay_task);
    unlink("test_async.txt");
    printf("[DEBUG] test_async_concurrent: cleanup complete\n");
}

void test_async_io() {
    InfraxCore* core = InfraxCoreClass.singleton();
    printf("Testing async I/O...\n");
    
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
    
    printf("Async I/O test passed\n");
}

void test_async_events() {
    InfraxCore* core = InfraxCoreClass.singleton();
    printf("Testing async events...\n");
    
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
    
    printf("Async events test passed\n");
}

int main(void) {
    printf("===================\nStarting InfraxAsync tests...\n");
    
    test_async_file_read();
    test_async_delay();
    test_async_concurrent();
    test_async_io();
    test_async_events();
    
    printf("All tests passed!\n===================\n");
    return 0;
}
