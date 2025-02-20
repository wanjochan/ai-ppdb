#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxLog.h"

InfraxCore* core = NULL;
InfraxMemory* memory = NULL;

// Test timeout control
#define TEST_TIMEOUT_MS 2000  // 2 seconds timeout for each test
#define POLL_INTERVAL_MS 10   // Initial poll interval
#define MAX_POLL_INTERVAL_MS 100  // Maximum poll interval

/* 使用系统 singal 的防范来辅助测试超时。。。
*/
static volatile int test_timeout = 0;

static void alarm_handler(int sig) {
    test_timeout = 1;
    core->printf(core, "Test timeout!\n");
}

static void setup_timeout(InfraxCore *core, unsigned int seconds) {
    test_timeout = 0;
    core->signal(core, INFRAX_SIGALRM, alarm_handler);
    core->alarm(core, seconds);
}

static void clear_timeout(InfraxCore *core) {
    core->alarm(core, 0);
    test_timeout = 0;
}

// Test context structure
typedef struct {
    int counter;
    int target;
    InfraxBool has_error;
    char error_msg[256];
} TestContext;

// File IO test context
typedef struct {
    const char* filename;
    InfraxHandle fd;
    void* buffer;
    InfraxSize buffer_size;
    InfraxSize bytes_processed;
    int yield_count;
} FileIOTestContext;

// Helper function to get current timestamp in milliseconds
static uint64_t get_current_time_ms(void) {
    return core->time_monotonic_ms(core);
}

// Timer test handlers
static void timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    char discard_buf[256];
    if (fd >= 0) {
        InfraxSize bytes_read;
        InfraxError err;
        while (INFRAX_ERROR_IS_OK(err = core->file_read(core, fd, discard_buf, sizeof(discard_buf), &bytes_read)) && bytes_read > 0) {}  // 完全清空管道
    }
    core->printf(core, "Timer event received!\n");
    *(int*)arg = 1;  // Set result
}

static void multi_timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    char discard_buf[256];
    if (fd >= 0) {
        while (core->read_fd(core, fd, discard_buf, sizeof(discard_buf)) > 0) {}  // 完全清空管道
    }
    int* timer_count = (int*)arg;
    (*timer_count)++;
    core->printf(NULL, "Timer %d fired!\n", *timer_count);
}

static void concurrent_timer_handler(InfraxAsync* self, int fd, short events, void* arg) {
    TestContext* ctx = (TestContext*)arg;
    char discard_buf[256];
    if (fd >= 0) {
        while (core->read_fd(core, fd, discard_buf, sizeof(discard_buf)) > 0) {}  // 完全清空管道
    }
    
    ctx->counter++;
    uint64_t now = get_current_time_ms();
    
    if (ctx->counter % 10 == 0 || ctx->counter == ctx->target) {
        core->printf(NULL, "Progress: %d/%d timers fired (%.2f%%)\n", 
                    ctx->counter, ctx->target,
                    (float)ctx->counter * 100 / ctx->target);
    }
}

// Timer tests
void test_async_timer() {
    core->printf(NULL,"Testing async with timer...\n");
    setup_timeout(core, 5);  // 5 second timeout
    
    // Create async task for polling
    InfraxAsync* async = InfraxAsyncClass.new(NULL, NULL);
    if (!async) {
        core->printf(NULL,"Failed to create async task\n");
        clear_timeout(core);
        return;
    }
    
    // Set timeout
    int result = 0;
    InfraxU32 timer_id = InfraxAsyncClass.setTimeout(1000, timer_handler, &result);
    if (timer_id == 0) {
        core->printf(NULL,"Failed to set timeout\n");
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    // Wait for timer event
    uint64_t start_time = get_current_time_ms();
    while (!result && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
        
        // 每秒打印一次进度
        static uint64_t last_progress = 0;
        uint64_t now = get_current_time_ms();
        if (now - last_progress >= 1000) {
            core->printf(NULL, "Waiting for timer... (elapsed: %lu ms)\n", 
                        now - start_time);
            last_progress = now;
        }
        
        // 检查是否超时
        if (now - start_time > 2000) {  // 2秒超时
            core->printf(NULL, "Timer did not expire in time\n");
            InfraxAsyncClass.clearTimeout(timer_id);
            InfraxAsyncClass.free(async);
            clear_timeout(core);
            return;
        }
    }
    
    if (test_timeout) {
        core->printf(NULL,"Test timed out\n");
        InfraxAsyncClass.clearTimeout(timer_id);
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    if (!result) {
        core->printf(NULL,"Timer did not expire in time\n");
        InfraxAsyncClass.clearTimeout(timer_id);
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    uint64_t end_time = get_current_time_ms();
    uint64_t elapsed = end_time - start_time;
    core->printf(NULL, "Timer test passed (elapsed: %lu ms)\n", elapsed);
    
    InfraxAsyncClass.clearTimeout(timer_id);
    InfraxAsyncClass.free(async);
    clear_timeout(core);
}

void test_multiple_timers() {
    core->printf(NULL, "Testing multiple concurrent timers...\n");
    setup_timeout(core, 10);  // 10 second timeout
    
    // Create async task for polling
    InfraxAsync* async = InfraxAsyncClass.new(NULL, NULL);
    if (!async) {
        core->printf(NULL,"Failed to create async task\n");
        clear_timeout(core);
        return;
    }
    
    int timer_count = 0;
    InfraxU32 timer1 = InfraxAsyncClass.setTimeout(500, multi_timer_handler, &timer_count);
    InfraxU32 timer2 = InfraxAsyncClass.setTimeout(1000, multi_timer_handler, &timer_count);
    
    if (timer1 == 0 || timer2 == 0) {
        core->printf(NULL, "Failed to set timers\n");
        if (timer1) InfraxAsyncClass.clearTimeout(timer1);
        if (timer2) InfraxAsyncClass.clearTimeout(timer2);
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    // Wait for all timers to fire
    uint64_t start_time = get_current_time_ms();
    while (timer_count < 2 && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
        
        // 每秒打印一次进度
        static uint64_t last_progress = 0;
        uint64_t now = get_current_time_ms();
        if (now - last_progress >= 1000) {
            core->printf(NULL, "Waiting for timers... (elapsed: %lu ms, count: %d/2)\n", 
                        now - start_time, timer_count);
            last_progress = now;
        }
        
        // 检查是否超时
        if (now - start_time > 3000) {  // 3秒超时
            core->printf(NULL, "Not all timers fired in time\n");
            InfraxAsyncClass.clearTimeout(timer1);
            InfraxAsyncClass.clearTimeout(timer2);
            InfraxAsyncClass.free(async);
            clear_timeout(core);
            return;
        }
    }
    
    if (test_timeout) {
        core->printf(NULL,"Test timed out\n");
        InfraxAsyncClass.clearTimeout(timer1);
        InfraxAsyncClass.clearTimeout(timer2);
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    if (timer_count != 2) {
        core->printf(NULL, "Not all timers fired (count=%d)\n", timer_count);
        InfraxAsyncClass.clearTimeout(timer1);
        InfraxAsyncClass.clearTimeout(timer2);
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    uint64_t end_time = get_current_time_ms();
    uint64_t elapsed = end_time - start_time;
    core->printf(NULL, "Multiple timers test passed (elapsed: %lu ms)\n", elapsed);
    
    InfraxAsyncClass.clearTimeout(timer1);
    InfraxAsyncClass.clearTimeout(timer2);
    InfraxAsyncClass.free(async);
    clear_timeout(core);
}

// 并发定时器测试
#define CONCURRENT_TIMER_COUNT 2048  // Increased from 300 to test higher concurrency

void test_concurrent_timers() {
    core->printf(NULL, "Testing %d concurrent timers...\n", CONCURRENT_TIMER_COUNT);
    setup_timeout(core, 60);  // 增加超时时间到60秒以适应更多的并发定时器
    
    // Create async task for polling
    InfraxAsync* async = InfraxAsyncClass.new(NULL, NULL);
    if (!async) {
        core->printf(NULL,"Failed to create async task\n");
        clear_timeout(core);
        return;
    }
    
    // 创建测试上下文
    TestContext ctx = {
        .counter = 0,
        .target = CONCURRENT_TIMER_COUNT,
        .has_error = INFRAX_FALSE
    };
    
    // 记录开始时间
    uint64_t start_time = get_current_time_ms();
    
    // 创建定时器数组
    InfraxU32 timer_ids[CONCURRENT_TIMER_COUNT];
    
    // 设置定时器，间隔从100ms到1000ms不等
    core->printf(NULL, "Creating %d timers...\n", CONCURRENT_TIMER_COUNT);
    for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
        InfraxU32 interval = 100 + (i % 10) * 100;  // 100ms到1000ms
        timer_ids[i] = InfraxAsyncClass.setTimeout(interval, concurrent_timer_handler, &ctx);
        if (timer_ids[i] == 0) {
            core->printf(NULL, "Failed to set timer %d\n", i);
            ctx.has_error = INFRAX_TRUE;
            core->snprintf(core, ctx.error_msg, sizeof(ctx.error_msg), "Failed to create timer %d", i);
            // 清理已创建的定时器
            for (int j = 0; j < i; j++) {
                InfraxAsyncClass.clearTimeout(timer_ids[j]);
            }
            InfraxAsyncClass.free(async);
            clear_timeout(core);
            return;
        }
    }
    core->printf(NULL, "All timers created successfully\n");
    
    // 等待所有定时器触发
    core->printf(NULL, "Waiting for timers to fire...\n");
    while (ctx.counter < CONCURRENT_TIMER_COUNT && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);
        
        // 每秒打印一次进度
        static uint64_t last_progress = 0;
        uint64_t now = get_current_time_ms();
        if (now - last_progress >= 1000) {
            core->printf(NULL, "Progress: %d/%d timers fired (%.2f%%)\n", 
                        ctx.counter, ctx.target,
                        (float)ctx.counter * 100 / ctx.target);
            last_progress = now;
        }
    }
    
    // 记录结束时间
    uint64_t end_time = get_current_time_ms();
    uint64_t total_time = end_time - start_time;
    
    if (test_timeout) {
        core->printf(NULL,"Test timed out after %lu ms. Only %d/%d timers fired.\n", 
                    total_time, ctx.counter, CONCURRENT_TIMER_COUNT);
        // 清理定时器
        for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
            InfraxAsyncClass.clearTimeout(timer_ids[i]);
        }
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    if (ctx.counter != CONCURRENT_TIMER_COUNT) {
        core->printf(NULL, "Not all timers fired (count=%d/%d) after %lu ms\n", 
                    ctx.counter, CONCURRENT_TIMER_COUNT, total_time);
        // 清理定时器
        for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
            InfraxAsyncClass.clearTimeout(timer_ids[i]);
        }
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    // 清理定时器
    for (int i = 0; i < CONCURRENT_TIMER_COUNT; i++) {
        InfraxAsyncClass.clearTimeout(timer_ids[i]);
    }
    
    InfraxAsyncClass.free(async);
    clear_timeout(core);
    
    // 打印性能统计
    core->printf(NULL, "\nPerformance Statistics:\n");
    core->printf(NULL, "Total time: %lu ms\n", total_time);
    core->printf(NULL, "Average time per timer: %.2f ms\n", 
                (float)total_time / CONCURRENT_TIMER_COUNT);
    core->printf(NULL, "Timers per second: %.2f\n", 
                (float)CONCURRENT_TIMER_COUNT * 1000 / total_time);
    
    core->printf(NULL, "Concurrent timers test passed\n");
}

// File IO callback
static void file_io_callback(InfraxAsync* self, int fd, short events, void* arg) {
    FileIOTestContext* ctx = (FileIOTestContext*)arg;
    InfraxCore* core = InfraxCoreClass.singleton();
    
    if (events & 0x001) {  // POLLIN
        // Read data
        ssize_t n = core->read_fd(core, fd, 
                        ctx->buffer + ctx->bytes_processed,
                        ctx->buffer_size - ctx->bytes_processed);
        
        if (n > 0) {
            ctx->bytes_processed += n;
            core->printf(core, "Read %zd bytes\n", n);
        }
    }
}

// Basic file IO test
void test_basic_file_io(void) {
    core->printf(core, "Testing basic file IO...\n");
    setup_timeout(core, 5);  // 5 second timeout
    
    // Create async instance to initialize pollset
    InfraxAsync* async = InfraxAsyncClass.new(NULL, NULL);
    if (!async) {
        core->printf(core, "Failed to create async instance\n");
        clear_timeout(core);
        return;
    }
    
    // Create test file with known content
    const char* test_file = "test_file.txt";
    const char* test_content = "Hello, File IO Test! This is a test content with known length.";
    InfraxHandle file_handle;
    InfraxError err = core->file_open(core, test_file, INFRAX_FILE_CREATE | INFRAX_FILE_WRONLY | INFRAX_FILE_TRUNC, 0644, &file_handle);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to create test file\n");
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    InfraxSize bytes_written;
    err = core->file_write(core, file_handle, test_content, core->strlen(core, test_content), &bytes_written);
    core->file_close(core, file_handle);
    
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to write test content\n");
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    
    // Initialize context
    FileIOTestContext ctx = {
        .filename = test_file,
        .fd = 0,
        .buffer = memory->alloc(memory, 1024),
        .buffer_size = 1024,
        .bytes_processed = 0,
        .yield_count = 0
    };
    
    if (!ctx.buffer) {
        core->printf(core, "Failed to allocate buffer\n");
        core->file_remove(core, test_file);
        InfraxAsyncClass.free(async);
        clear_timeout(core);
        return;
    }
    core->memset(core, ctx.buffer, 0, ctx.buffer_size);
    
    // Open file for reading
    err = core->file_open(core, test_file, INFRAX_FILE_RDONLY, 0, &ctx.fd);
    if (INFRAX_ERROR_IS_ERR(err)) {
        core->printf(core, "Failed to open file for reading\n");
        goto cleanup;
    }
    
    // Add to pollset
    if (InfraxAsyncClass.pollset_add_fd(async, ctx.fd, 0x001, file_io_callback, &ctx) < 0) {
        core->printf(core, "Failed to add fd to pollset\n");
        goto cleanup;
    }
    
    // Poll until read complete or timeout
    uint64_t start_time = get_current_time_ms();
    InfraxSize expected_size = core->strlen(core, test_content);
    
    while (ctx.bytes_processed < expected_size && !test_timeout) {
        InfraxAsyncClass.pollset_poll(async, 100);  // 100ms timeout
        
        // Check for timeout
        uint64_t now = get_current_time_ms();
        if (now - start_time > 3000) {  // 3 second operation timeout
            core->printf(core, "Operation timed out\n");
            goto cleanup;
        }
    }
    
    // Verify result
    if (test_timeout) {
        core->printf(core, "Test timed out\n");
        goto cleanup;
    }
    
    if (ctx.bytes_processed != expected_size) {
        core->printf(core, "Read size mismatch: expected %zu, got %zu\n", 
                     expected_size, ctx.bytes_processed);
        goto cleanup;
    }
    
    if (core->memcmp(core, ctx.buffer, test_content, expected_size) != 0) {
        core->printf(core, "Content verification failed\n");
        goto cleanup;
    }
    
    core->printf(core, "File IO test passed: Read %zu bytes, content verified\n", 
                 ctx.bytes_processed);
    
cleanup:
    if (ctx.fd) {
        core->file_close(core, ctx.fd);
    }
    if (ctx.buffer) {
        memory->dealloc(memory, ctx.buffer);
    }
    if (async) {
        InfraxAsyncClass.free(async);
    }
    core->file_remove(core, test_file);
    clear_timeout(core);
}

int main(void) {
    core = InfraxCoreClass.singleton();
    if (!core) {
        core->printf(core, "Failed to get core singleton\n");
        return 1;
    }
    
    // 初始化内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = INFRAX_FALSE,
        .use_pool = INFRAX_TRUE,
        .gc_threshold = 0
    };
    memory = InfraxMemoryClass.new(&mem_config);
    if (!memory) {
        core->printf(core, "Failed to create memory manager\n");
        return 1;
    }
    
    test_async_timer();
    test_multiple_timers();
    test_concurrent_timers();
    test_basic_file_io();
    
    InfraxMemoryClass.free(memory);
    return 0;
}
