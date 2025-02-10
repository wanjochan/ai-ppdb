#include "internal/infrax/InfraxCore.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxLog.h"

// static int test_get_max_file_descriptors(void) {
//     // 支持高并发的默认值
//     // 1. 65536 (64K) 足够支持中等规模并发
//     // 2. 如果需要百万级并发，建议通过配置调整系统限制
//     // 3. 实际值应该根据系统配置和应用需求来设定
//     return 65536;  // 基础值设为 64K
// }

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
    
    // Wait for completion
    while (async->state == INFRAX_ASYNC_PENDING) {
        // Task will handle its own timing
        InfraxCore* core = InfraxCoreClass.singleton();
        core->sleep_ms(core, 10);//这里是错的？
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
    
    // Wait for completion
    while (async->state == INFRAX_ASYNC_PENDING) {
        // Task will handle its own timing
        InfraxCore* core = InfraxCoreClass.singleton();
        core->sleep_ms(core, 10);//TODO coding the async timer, wait for it.
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
    
    // Wait for both tasks to complete
    while (read_task->state == INFRAX_ASYNC_PENDING || 
           delay_task->state == INFRAX_ASYNC_PENDING) {
        // Tasks will handle their own timing
        InfraxCore* core = InfraxCoreClass.singleton();
        core->sleep_ms(core, 10);
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

// Test async function
static void test_async_fn(InfraxAsync* self, void* arg) {
    printf("Test async function started\n");
    InfraxAsyncClass.yield(self);
    printf("Test async function resumed\n");
}

// Test poll callback
static void test_poll_callback(int fd, short revents, void* arg) {
    if (revents & INFRAX_POLLIN) {
        char buf[128];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Poll callback received: %s\n", buf);
        }
    }
}

// Main test function
int main(void) {
    InfraxLog* log = InfraxLogClass.singleton();
    log->info(log, "Starting InfraxAsync tests...");
    
    // int maxfd = test_get_max_file_descriptors();
    // log->info(log, "maxfd=%d\n", maxfd);
    
    test_async_file_read();
    test_async_delay();
    test_async_concurrent();
    
    printf("\n=== Testing InfraxAsync ===\n\n");
    
    // Test 1: Basic async task
    printf("Test 1: Basic async task\n");
    InfraxAsync* async = InfraxAsyncClass.new(test_async_fn, NULL);
    if (!async) {
        printf("Failed to create async task\n");
        return 1;
    }
    
    InfraxAsyncClass.start(async);
    printf("Async task started\n");
    
    InfraxAsyncClass.start(async);  // Resume
    printf("Async task completed\n");
    
    // Test 2: Pollset
    printf("\nTest 2: Pollset\n");
    
    // Create pipe for testing
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        printf("Failed to create pipe\n");
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Set non-blocking
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    
    // Add read end to pollset
    if (InfraxAsyncClass.pollset_add_fd(async, pipefd[0], INFRAX_POLLIN, test_poll_callback, NULL) != 0) {
        printf("Failed to add fd to pollset\n");
        close(pipefd[0]);
        close(pipefd[1]);
        InfraxAsyncClass.free(async);
        return 1;
    }
    
    // Write some data
    const char* test_data = "Hello, Poll!";
    write(pipefd[1], test_data, strlen(test_data));
    
    // Poll for events
    printf("Polling for events...\n");
    InfraxAsyncClass.pollset_poll(async, 1000);
    
    // Remove fd from pollset
    InfraxAsyncClass.pollset_remove_fd(async, pipefd[0]);
    
    // Cleanup
    close(pipefd[0]);
    close(pipefd[1]);
    InfraxAsyncClass.free(async);
    
    printf("\n=== All tests completed ===\n");
    return 0;
}
