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
#define DELAY_SECONDS 3

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
static void async_read_file(void* arg) {
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
                infrax_async_yield(NULL);  // Would block, yield to caller
                continue;
            }
            // Real error
            break;
        }
        
        if (n == 0) break;  // EOF
        
        ctx->bytes_read += n;
        ctx->yield_count++;  // Count yield after successful read
        infrax_async_yield(NULL);  // Yield after each successful read
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
    
    // Start async read
    InfraxAsync* async = infrax_async_start(async_read_file, &ctx);
    assert(async != NULL);
    
    // Wait for completion
    int result = infrax_async_wait(async);
    assert(result == 0);
    
    // Verify content
    assert(strncmp(buffer, TEST_CONTENT, strlen(TEST_CONTENT)) == 0);
    // Verify that yield was called at least once
    assert(ctx.yield_count > 0);
    printf("Async read test passed: content matches, yielded %d times\n", ctx.yield_count);
    
    // Cleanup
    unlink(TEST_FILE);
}

// Async delay function
static void async_delay(void* arg) {
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);
    time_t elapsed = 0;
    int yield_count = 0;
    
    while (elapsed < DELAY_SECONDS) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        elapsed = current.tv_sec - start.tv_sec;
        yield_count++;  // Count yield
        infrax_async_yield(NULL);  // Yield on each iteration
        usleep(100000);  // Sleep for 100ms to avoid tight loop
    }
    printf("Delay task yielded %d times\n", yield_count);
}

// Test async delay
static void test_async_delay(void) {
    printf("Starting delay test (will wait for %d seconds)...\n", DELAY_SECONDS);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Start async delay
    InfraxAsync* async = infrax_async_start(async_delay, NULL);
    assert(async != NULL);
    
    // Wait for completion
    int result = infrax_async_wait(async);
    assert(result == 0);
    
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_t elapsed = end.tv_sec - start.tv_sec;
    
    // Verify that approximately DELAY_SECONDS has passed
    assert(elapsed >= DELAY_SECONDS && elapsed <= DELAY_SECONDS + 1);
    printf("Async delay test passed: waited for %ld seconds\n", elapsed);
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
    
    // Start both tasks
    printf("Starting file read and delay tasks...\n");
    InfraxAsync* read_task = infrax_async_start(async_read_file, &ctx);
    InfraxAsync* delay_task = infrax_async_start(async_delay, NULL);
    assert(read_task != NULL && delay_task != NULL);
    
    // Record end time
    time_t end_time = time(NULL);
    
    // Verify results
    assert(strncmp(buffer, TEST_CONTENT, strlen(TEST_CONTENT)) == 0);
    assert(end_time - start_time >= DELAY_SECONDS);
    
    printf("Concurrent test completed! Total time: %ld seconds\n", end_time - start_time);
    
    // Wait for tasks to clean up resources
    infrax_async_wait(read_task);
    infrax_async_wait(delay_task);
    
    // Cleanup
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
