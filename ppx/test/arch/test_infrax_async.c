#include "internal/infrax/InfraxAsync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#define TEST_FILE "test_async.txt"
#define TEST_CONTENT "Hello, Async World!"
#define DELAY_SECONDS 0.5  // Reduce delay time for testing

// Test context structure
typedef struct {
    int fd;             // File descriptor
    char* buffer;       // Read buffer
    size_t size;        // Buffer size
    size_t bytes_read;  // Total bytes read
    const char* filename;
    int yield_count;    // Count how many times yield is called
} ReadContext;

// Non-blocking async file read function
static void async_read_file(InfraxAsync* self, void* arg) {
    ReadContext* ctx = (ReadContext*)arg;
    
    // First call: open file
    ctx->fd = open(ctx->filename, O_RDONLY | O_NONBLOCK);
    if (ctx->fd < 0) return;
    
    // Keep reading until complete
    while (ctx->bytes_read < ctx->size) {
        ssize_t n = read(ctx->fd, 
                        ctx->buffer + ctx->bytes_read, 
                        ctx->size - ctx->bytes_read);
                     
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ctx->yield_count++;  // Count yield on would block
                self->yield(self);  // Would block, yield to caller
                continue;
            }
            // Real error
            break;
        }
        
        if (n == 0) break;  // EOF
        
        ctx->bytes_read += n;
        ctx->yield_count++;  // Count yield after successful read
        self->yield(self);  // Yield after each successful read
    }
    
    // Done or error
    close(ctx->fd);
    ctx->fd = -1;
}

// Test async file operations
static void test_async_file_read(void) {
    // Create a test file
    FILE* f = fopen(TEST_FILE, "w");
    assert(f != NULL);
    fputs(TEST_CONTENT, f);
    fclose(f);
    
    // Prepare read context
    char buffer[128] = {0};
    ReadContext ctx = {
        .fd = -1,
        .buffer = buffer,
        .size = sizeof(buffer),
        .bytes_read = 0,
        .filename = TEST_FILE,
        .yield_count = 0
    };
    
    // Create and start async task
    InfraxAsync* async = InfraxAsync_CLASS.new(async_read_file, &ctx);
    assert(async != NULL);
    async->start(async, async_read_file, &ctx);
    
    // Wait for completion by polling
    while (async->status(async) != INFRAX_ASYNC_DONE &&
           async->status(async) != INFRAX_ASYNC_ERROR) {
        usleep(1000);  // Small delay to prevent CPU overload
    }
    
    // Verify content
    assert(strncmp(buffer, TEST_CONTENT, strlen(TEST_CONTENT)) == 0);
    // Verify that yield was called at least once
    assert(ctx.yield_count > 0);
    printf("Async read test passed: content matches, yielded %d times\n", ctx.yield_count);
    
    // Cleanup
    InfraxAsync_CLASS.free(async);
    unlink(TEST_FILE);
}

// Async delay function
static void async_delay(InfraxAsync* self, void* arg) {
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double elapsed = 0;
    int yield_count = 0;
    
    printf("Delay task started\n");
    while (elapsed < DELAY_SECONDS) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        elapsed = (current.tv_sec - start.tv_sec) + 
                 (current.tv_nsec - start.tv_nsec) / 1e9;
        yield_count++;
        printf("Delay task yielding: %.3f seconds elapsed\n", elapsed);
        self->yield(self);
        usleep(10000);  // 10ms sleep
    }
    printf("Delay task completed after %d yields\n", yield_count);
}

// Test async delay
static void test_async_delay(void) {
    printf("Starting delay test (will wait for %.3f seconds)...\n", DELAY_SECONDS);
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Create and start async delay task
    InfraxAsync* async = InfraxAsync_CLASS.new(async_delay, NULL);
    assert(async != NULL);
    async->start(async, async_delay, NULL);
    
    // Wait for completion by polling
    int poll_count = 0;
    while (async->status(async) != INFRAX_ASYNC_DONE &&
           async->status(async) != INFRAX_ASYNC_ERROR) {
        usleep(1000);  // 1ms sleep
        poll_count++;
        if (poll_count % 100 == 0) {  // Print status every 100ms
            clock_gettime(CLOCK_MONOTONIC, &current);
            double elapsed = (current.tv_sec - start.tv_sec) + 
                           (current.tv_nsec - start.tv_nsec) / 1e9;
            printf("Waiting... %.3f seconds elapsed, status: %d\n", 
                   elapsed, async->status(async));
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &current);
    double elapsed = (current.tv_sec - start.tv_sec) + 
                    (current.tv_nsec - start.tv_nsec) / 1e9;
    
    // Verify that approximately DELAY_SECONDS has passed
    assert(elapsed >= DELAY_SECONDS);
    assert(elapsed <= DELAY_SECONDS + 0.1);  // Allow 100ms margin
    printf("Async delay test passed: waited for %.3f seconds\n", elapsed);
    
    // Cleanup
    InfraxAsync_CLASS.free(async);
}

// Test concurrent async operations
static void test_async_concurrent(void) {
    // Prepare read context
    char buffer[128] = {0};
    ReadContext ctx = {
        .fd = -1,
        .buffer = buffer,
        .size = sizeof(buffer),
        .bytes_read = 0,
        .filename = TEST_FILE,
        .yield_count = 0
    };
    
    // Create test file
    FILE* f = fopen(TEST_FILE, "w");
    assert(f != NULL);
    fputs(TEST_CONTENT, f);
    fclose(f);
    
    // Record start time
    time_t start_time = time(NULL);
    
    // Create and start both tasks
    printf("Starting file read and delay tasks...\n");
    InfraxAsync* read_task = InfraxAsync_CLASS.new(async_read_file, &ctx);
    InfraxAsync* delay_task = InfraxAsync_CLASS.new(async_delay, NULL);
    assert(read_task != NULL && delay_task != NULL);
    
    read_task->start(read_task, async_read_file, &ctx);
    delay_task->start(delay_task, async_delay, NULL);
    
    // Wait for both tasks to complete
    while ((read_task->status(read_task) != INFRAX_ASYNC_DONE &&
            read_task->status(read_task) != INFRAX_ASYNC_ERROR) ||
           (delay_task->status(delay_task) != INFRAX_ASYNC_DONE &&
            delay_task->status(delay_task) != INFRAX_ASYNC_ERROR)) {
        usleep(1000);
    }
    
    // Record end time
    time_t end_time = time(NULL);
    
    // Verify results
    assert(strncmp(buffer, TEST_CONTENT, strlen(TEST_CONTENT)) == 0);
    assert(end_time - start_time >= DELAY_SECONDS);
    
    printf("Concurrent test completed! Total time: %ld seconds\n", end_time - start_time);
    
    // Cleanup
    InfraxAsync_CLASS.free(read_task);
    InfraxAsync_CLASS.free(delay_task);
    unlink(TEST_FILE);
}

int main(void) {
    printf("Starting InfraxAsync tests...\n");
    
    test_async_file_read();
    test_async_delay();
    test_async_concurrent();
    
    printf("All tests passed!\n");
    return 0;
}
