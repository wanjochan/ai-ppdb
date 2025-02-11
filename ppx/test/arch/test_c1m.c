#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"

static InfraxCore* core = NULL;
static InfraxMemory* memory = NULL;

#define TARGET_CONNECTIONS 1000
#define BATCH_SIZE 50  // 减小初始批次大小
#define TEST_DURATION_SEC 30
#define TASK_LIFETIME_MS 500  // 减少任务生命周期
#define MAX_YIELD_COUNT 50000  // 减少yield次数限制

// Performance metrics
typedef struct {
    size_t active_tasks;
    size_t completed_tasks;
    size_t failed_tasks;
    double avg_response_time;
    size_t peak_memory;
    InfraxTimeSpec start_time;
    double cpu_usage;
    size_t total_memory;
    size_t peak_active_tasks;  // 新增：记录峰值并发数
} TestMetrics;

TestMetrics metrics = {0};

// Get CPU usage
double get_cpu_usage() {
    static clock_t last_cpu = 0;
    static InfraxTimeSpec last_time = {0};
    
    clock_t current_cpu = core->clock(core);
    InfraxTimeSpec current_time;
    core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &current_time);
    
    if (last_cpu == 0) {
        last_cpu = current_cpu;
        last_time = current_time;
        return 0.0;
    }
    
    double cpu_time = (double)(current_cpu - last_cpu) / core->clocks_per_sec(core);
    double real_time = (current_time.tv_sec - last_time.tv_sec) + 
                      (current_time.tv_nsec - last_time.tv_nsec) / 1e9;
    
    last_cpu = current_cpu;
    last_time = current_time;
    
    return (cpu_time / real_time) * 100.0;
}

// Task context structure
typedef struct {
    InfraxAsync* async;
    InfraxTimeSpec start_time;
    int is_active;
    int state;  // 0: initial, 1: running, 2: completed
} TaskContext;

// Task cleanup function
void cleanup_task(TaskContext* ctx) {
    if (ctx) {
        if (ctx->async) {
            if (!InfraxAsyncClass.is_done(ctx->async)) {
                InfraxAsyncClass.cancel(ctx->async);  // 先取消任务
                // 等待任务完全停止，使用 yield 而不是 sleep
                while (!InfraxAsyncClass.is_done(ctx->async)) {
                    InfraxAsyncClass.yield(ctx->async);  // 使用异步 yield 替代阻塞的 sleep
                }
            }
            InfraxAsyncClass.free(ctx->async);
        }
        memory->dealloc(memory, ctx);
        __atomic_fetch_sub(&metrics.active_tasks, 1, __ATOMIC_SEQ_CST);
    }
}

// Long running task function
void long_running_task(InfraxAsync* self, void* arg) {
    if (!self || !arg) return;  // 参数检查
    
    TaskContext* ctx = (TaskContext*)arg;
    static __thread int yield_count = 0;
    
    // First time entry
    if (ctx->state == 0) {
        ctx->state = 1;
        core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &ctx->start_time);
        yield_count = 0;
    }
    
    while (self->state == INFRAX_ASYNC_PENDING) {
        // Check if we've run long enough
        InfraxTimeSpec current_time;
        core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &current_time);
        long elapsed_ms = (current_time.tv_sec - ctx->start_time.tv_sec) * 1000 +
                         (current_time.tv_nsec - ctx->start_time.tv_nsec) / 1000000;
                         
        if (elapsed_ms >= TASK_LIFETIME_MS) {
            // Task completed
            ctx->state = 2;
            __atomic_fetch_add(&metrics.completed_tasks, 1, __ATOMIC_SEQ_CST);
            self->state = INFRAX_ASYNC_FULFILLED;
            return;
        }
        
        // Increment yield count with bounds checking
        if (yield_count < MAX_YIELD_COUNT) {
            yield_count++;
            InfraxAsyncClass.yield(self);
        } else {
            // Task has yielded too many times, terminate it
            self->state = INFRAX_ASYNC_REJECTED;
            __atomic_fetch_add(&metrics.failed_tasks, 1, __ATOMIC_SEQ_CST);
            return;
        }
    }
}

// Get current memory usage
size_t get_memory_usage() {
    return core->get_memory_usage(core);
}

// Process active tasks
void process_active_tasks() {
    static int cycle_count = 0;
    cycle_count++;
    
    // 使用 yield 来让出执行权，而不是阻塞的 sleep
    if (cycle_count % 50 == 0 && metrics.active_tasks > TARGET_CONNECTIONS * 0.95) {
        core->yield(core);  // 使用非阻塞的 yield
    }
}

// Create and start a batch of timer tasks
void create_task_batch(size_t target_tasks) {
    if (!memory || !core) return;  // 安全检查
    
    size_t current_active = metrics.active_tasks;
    size_t to_create = target_tasks > current_active ? target_tasks - current_active : 0;
    
    // Dynamic batch size based on system load
    size_t batch_size = BATCH_SIZE;
    if (current_active > TARGET_CONNECTIONS * 0.9) {
        batch_size = BATCH_SIZE / 4;  // Reduce batch size under high load
    } else if (current_active > TARGET_CONNECTIONS * 0.7) {
        batch_size = BATCH_SIZE / 2;  // Moderate batch size under medium load
    }
    
    to_create = to_create > batch_size ? batch_size : to_create;
    
    for (size_t i = 0; i < to_create && metrics.active_tasks < TARGET_CONNECTIONS; i++) {
        TaskContext* ctx = memory->alloc(memory, sizeof(TaskContext));
        if (!ctx) continue;
        
        memset(ctx, 0, sizeof(TaskContext));
        core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &ctx->start_time);
        ctx->is_active = 1;
        
        ctx->async = InfraxAsyncClass.new(long_running_task, ctx);
        if (!ctx->async) {
            memory->dealloc(memory, ctx);
            continue;
        }
        
        // 先增加计数，再启动任务
        size_t current_active = __atomic_fetch_add(&metrics.active_tasks, 1, __ATOMIC_SEQ_CST) + 1;
        if (current_active > metrics.peak_active_tasks) {
            metrics.peak_active_tasks = current_active;
        }
        
        if (!InfraxAsyncClass.start(ctx->async)) {  // 检查启动结果
            __atomic_fetch_sub(&metrics.active_tasks, 1, __ATOMIC_SEQ_CST);
            cleanup_task(ctx);
            continue;
        }
        
        if (ctx->async->state == INFRAX_ASYNC_REJECTED) {
            __atomic_fetch_add(&metrics.failed_tasks, 1, __ATOMIC_SEQ_CST);
            cleanup_task(ctx);
        }
    }
}

// Print current metrics
void print_metrics(time_t elapsed_seconds) {
    metrics.cpu_usage = get_cpu_usage();
    metrics.total_memory = get_memory_usage();
    if (metrics.total_memory > metrics.peak_memory) {
        metrics.peak_memory = metrics.total_memory;
    }
    
    core->printf(core, "\033[2J\033[H");  // 清屏并移到开头
    core->printf(core, "=== Test Progress: %ld/%d seconds ===\n", elapsed_seconds, TEST_DURATION_SEC);
    core->printf(core, "Current Active Tasks: %zu\n", metrics.active_tasks);
    core->printf(core, "Peak Active Tasks:   %zu\n", metrics.peak_active_tasks);
    core->printf(core, "Completed Tasks:     %zu\n", metrics.completed_tasks);
    core->printf(core, "Failed Tasks:        %zu\n", metrics.failed_tasks);
    core->printf(core, "CPU Usage:           %.1f%%\n", metrics.cpu_usage);
    core->printf(core, "Current Memory:      %.2f MB\n", metrics.total_memory / 1024.0);
    core->printf(core, "Peak Memory:         %.2f MB\n", metrics.peak_memory / 1024.0);
    core->printf(core, "Tasks/sec:           %.2f\n", metrics.completed_tasks / (double)elapsed_seconds);
    core->printf(core, "----------------------------------------\n");
}

int main() {
    printf("Initializing core...\n");
    core = InfraxCoreClass.singleton();
    if (!core) {
        printf("Failed to initialize core!\n");
        return 1;
    }
    
    printf("Initializing memory...\n");
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB initial size
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    memory = InfraxMemoryClass.new(&config);
    if (!memory) {
        printf("Failed to initialize memory!\n");
        return 1;
    }
    
    printf("Initialization completed, starting test...\n");
    core->printf(core, "Starting 1K Concurrent Tasks Test...\n");
    core->printf(core, "Target Connections: %d\n", TARGET_CONNECTIONS);
    core->printf(core, "Test Duration: %d seconds\n", TEST_DURATION_SEC);
    core->printf(core, "Task Lifetime: %d ms\n", TASK_LIFETIME_MS);
    core->printf(core, "----------------------------------------\n");
    core->yield(core);  // 使用 yield 替代 sleep

    core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &metrics.start_time);
    time_t start_time = core->time(core, NULL);
    time_t current_time;
    time_t last_print_time = 0;

    // Main test loop
    while ((current_time = core->time(core, NULL)) - start_time < TEST_DURATION_SEC) {
        create_task_batch(TARGET_CONNECTIONS);
        process_active_tasks();
        
        // 每秒更新一次指标
        if (current_time != last_print_time) {
            print_metrics(current_time - start_time);
            last_print_time = current_time;
        }

        core->yield(core);  // 使用 yield 替代 sleep
    }

    // Final metrics
    core->printf(core, "\nTest Completed!\n");
    print_metrics(TEST_DURATION_SEC);
    
    return 0;
}
