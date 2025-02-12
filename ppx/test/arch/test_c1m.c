#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"

static InfraxCore* core = NULL;
static InfraxMemory* memory = NULL;

// 添加任务数组管理
static InfraxAsync** tasks = NULL;
static size_t tasks_capacity = 0;

#define TARGET_CONNECTIONS 10     // 从1000改为10
#define BATCH_SIZE 2             // 从20改为2
#define TEST_DURATION_SEC 10     // 从30改为10
#define TASK_LIFETIME_MS 2000    // 从500改为2000
#define COMPUTATION_LIMIT 1000   // 从100改为1000

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
    size_t peak_active_tasks;
    InfraxTime last_batch_time;    // 上一批次的创建时间
    size_t tasks_per_second;       // 目标每秒创建任务数
    size_t current_batch_count;    // 当前批次已创建数量
} TestMetrics;

TestMetrics metrics = {0};

// Task context structure
typedef struct {
    InfraxAsync* async;
    InfraxTimeSpec start_time;
    int is_active;
    int state;  // 0: initial, 1: running, 2: completed
    int computation_count;  // 计算次数
} TaskContext;

// Forward declarations
static void long_running_task(InfraxAsync* self, void* arg);
static void cleanup_task(TaskContext* ctx);
static double get_cpu_usage(void);
static size_t get_memory_usage(void);
static void process_active_tasks(void);
static void create_task_batch(size_t target_tasks);
static void print_metrics(time_t elapsed_seconds);

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

// Task cleanup function
void cleanup_task(TaskContext* ctx) {
    if (!ctx) return;
    
    if (ctx->async) {
        if (!InfraxAsyncClass.is_done(ctx->async)) {
            InfraxAsyncClass.cancel(ctx->async);
        }
        InfraxAsyncClass.free(ctx->async);
        ctx->async = NULL;  // 防止重复释放
    }
    
    memory->dealloc(memory, ctx);
}

// Process active tasks
void process_active_tasks() {
    static InfraxTime last_process_time = 0;
    InfraxTime now = core->time_monotonic_ms(core);
    
    // 限制处理频率
    if (now - last_process_time < 10) {  // 每10ms处理一次
        return;
    }
    last_process_time = now;
    
    size_t active_count = 0;  // 实时计算活跃任务数
    
    // 遍历所有任务
    for (size_t i = 0; i < tasks_capacity; i++) {
        if (!tasks[i]) continue;  // 跳过空槽位
        
        if (InfraxAsyncClass.is_done(tasks[i])) {
            // 任务已完成，进行清理
            TaskContext* ctx = (TaskContext*)tasks[i]->arg;
            if (tasks[i]->state == INFRAX_ASYNC_FULFILLED) {
                __atomic_fetch_add(&metrics.completed_tasks, 1, __ATOMIC_SEQ_CST);
            } else {
                __atomic_fetch_add(&metrics.failed_tasks, 1, __ATOMIC_SEQ_CST);
            }
            cleanup_task(ctx);
            tasks[i] = NULL;
        } else {
            // 任务仍在运行
            active_count++;
            InfraxAsyncClass.pollset_poll(tasks[i], 1);
            if (tasks[i]->state == INFRAX_ASYNC_PENDING) {
                InfraxAsyncClass.start(tasks[i]);
            }
        }
    }
    
    // 原子更新活跃任务数
    __atomic_store_n(&metrics.active_tasks, active_count, __ATOMIC_SEQ_CST);
}

// Create a batch of tasks with rate limiting
static void create_task_batch(size_t target_tasks) {
    InfraxTime now = core->time_now_ms(core);
    
    // 计算时间窗口内应该创建的任务数
    InfraxTime time_delta = now - metrics.last_batch_time;
    if (time_delta < 1) return;  // 时间间隔太短，跳过本次创建
    
    // 计算本次可以创建的任务数
    size_t max_new_tasks = (metrics.active_tasks >= TARGET_CONNECTIONS * 0.8) ? 
        1 : // 高负载时每次只创建1个
        BATCH_SIZE; // 正常负载使用标准批次大小
    
    // 确保任务数组容量足够
    if (tasks_capacity < TARGET_CONNECTIONS) {
        size_t new_capacity = TARGET_CONNECTIONS * 2;
        InfraxAsync** new_tasks = memory->alloc(memory, sizeof(InfraxAsync*) * new_capacity);
        if (!new_tasks) return;
        
        if (tasks) {
            memcpy(new_tasks, tasks, sizeof(InfraxAsync*) * tasks_capacity);
            memory->dealloc(memory, tasks);
        }
        tasks = new_tasks;
        tasks_capacity = new_capacity;
    }
    
    // 创建任务
    size_t created = 0;
    for (size_t i = 0; i < max_new_tasks && metrics.active_tasks < TARGET_CONNECTIONS; i++) {
        // 找一个空槽位
        size_t slot;
        for (slot = 0; slot < tasks_capacity; slot++) {
            if (!tasks[slot]) break;
        }
        if (slot >= tasks_capacity) break;
        
        TaskContext* ctx = memory->alloc(memory, sizeof(TaskContext));
        if (!ctx) continue;
        
        // 初始化上下文
        memset(ctx, 0, sizeof(TaskContext));
        ctx->is_active = 1;
        ctx->state = 0;
        core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &ctx->start_time);
        
        // 创建异步任务
        InfraxAsync* async = InfraxAsyncClass.new(long_running_task, ctx);
        if (!async) {
            memory->dealloc(memory, ctx);
            continue;
        }
        
        ctx->async = async;
        tasks[slot] = async;
        
        // 启动任务
        if (!InfraxAsyncClass.start(async)) {
            cleanup_task(ctx);
            tasks[slot] = NULL;
            continue;
        }
        
        created++;
        __atomic_fetch_add(&metrics.active_tasks, 1, __ATOMIC_SEQ_CST);
        if (metrics.active_tasks > metrics.peak_active_tasks) {
            metrics.peak_active_tasks = metrics.active_tasks;
        }
    }
    
    // 更新指标
    metrics.last_batch_time = now;
    metrics.current_batch_count += created;
}

// Long running task function
void long_running_task(InfraxAsync* self, void* arg) {
    if (!self || !arg) return;  // 参数检查
    
    TaskContext* ctx = (TaskContext*)arg;
    
    // First time entry
    if (ctx->state == 0) {
        ctx->state = 1;
        core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &ctx->start_time);
        ctx->computation_count = 0;
        core->printf(core, "Task started\n");
    }
    
    // Check if we've run long enough
    InfraxTimeSpec current_time;
    core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &current_time);
    long elapsed_ms = (current_time.tv_sec - ctx->start_time.tv_sec) * 1000 +
                     (current_time.tv_nsec - ctx->start_time.tv_nsec) / 1000000;
                     
    if (elapsed_ms >= TASK_LIFETIME_MS) {
        // Task completed
        ctx->state = 2;
        self->state = INFRAX_ASYNC_FULFILLED;
        core->printf(core, "Task completed after %ld ms\n", elapsed_ms);
        return;
    }
    
    // 添加一些实际工作
    for(volatile int i = 0; i < 100; i++) {  // 减少每次的计算量
        // 模拟一些计算工作
        volatile int x = i * i;
    }
    
    // 增加计算次数
    ctx->computation_count++;
    
    // 如果计算次数超过限制，就结束任务
    if (ctx->computation_count >= COMPUTATION_LIMIT) {
        core->printf(core, "Task failed: too many computations (%d)\n", ctx->computation_count);
        self->state = INFRAX_ASYNC_REJECTED;
        return;
    }
    
    // 返回让其他任务运行
    self->state = INFRAX_ASYNC_PENDING;  // 确保任务保持在pending状态
    
    core->hint_yield(core);//hint cpu to yield?
    return;
}

// Get current memory usage
size_t get_memory_usage() {
    return core->get_memory_usage(core);
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
        .initial_size = 64 * 1024 * 1024,  // 64MB initial size
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024 * 1024  // 32MB
    };
    memory = InfraxMemoryClass.new(&config);
    if (!memory) {
        printf("Failed to initialize memory!\n");
        return 1;
    }
    
    // 初始化任务数组
    tasks_capacity = TARGET_CONNECTIONS * 2;
    tasks = memory->alloc(memory, sizeof(InfraxAsync*) * tasks_capacity);
    if (!tasks) {
        printf("Failed to allocate tasks array!\n");
        InfraxMemoryClass.free(memory);
        return 1;
    }
    memset(tasks, 0, sizeof(InfraxAsync*) * tasks_capacity);
    
    printf("Initialization completed, starting test...\n");
    
    // Initialize metrics
    core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &metrics.start_time);
    metrics.last_batch_time = core->time_now_ms(core);
    metrics.tasks_per_second = TARGET_CONNECTIONS / 10;  // 目标10秒内达到连接数
    
    time_t last_print_time = 0;
    
    while (1) {
        InfraxTimeSpec current_time;
        core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &current_time);
        time_t elapsed_seconds = current_time.tv_sec - metrics.start_time.tv_sec;
        
        if (elapsed_seconds >= TEST_DURATION_SEC) break;
        
        // 处理现有任务
        process_active_tasks();
        
        // 非阻塞地创建新任务
        create_task_batch(TARGET_CONNECTIONS);
        
        // 定期打印指标（避免过于频繁）
        if (elapsed_seconds > last_print_time) {
            print_metrics(elapsed_seconds);
            last_print_time = elapsed_seconds;
        }
        
        // 让出控制权给事件循环
        InfraxAsyncClass.pollset_poll(NULL, 0);
    }
    
    // Print final metrics
    print_metrics(TEST_DURATION_SEC);
    
    // Cleanup
    printf("\nTest completed. Cleaning up...\n");
    
    // 确保所有任务都被处理
    InfraxTime cleanup_start = core->time_monotonic_ms(core);
    while (1) {
        process_active_tasks();
        
        // 检查是否所有任务都已清理
        bool all_slots_empty = true;
        for (size_t i = 0; i < tasks_capacity; i++) {
            if (tasks[i] != NULL) {
                all_slots_empty = false;
                break;
            }
        }
        
        if (all_slots_empty) break;
        
        // 防止清理死循环
        InfraxTime current = core->time_monotonic_ms(core);
        if (current - cleanup_start > 5000) {  // 5秒超时保护
            printf("Cleanup timeout, forcing exit...\n");
            break;
        }
        
        InfraxAsyncClass.pollset_poll(NULL, 10);
    }
    
    if (memory) {
        InfraxMemoryClass.free(memory);
    }
    
    return 0;
}
