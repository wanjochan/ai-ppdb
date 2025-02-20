#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"

static InfraxCore* core = NULL;
static InfraxMemory* memory = NULL;

// 添加任务数组管理
static InfraxAsync* task_pool[200] = {0};  // 增加任务池大小到200
static size_t task_pool_index = 0;

#define TARGET_CONNECTIONS 150    // 增加目标连接数
#define BATCH_SIZE 50            // 增加批处理大小
#define TEST_DURATION_SEC 30     // 保持30秒
#define TASK_LIFETIME_MS 1000    // 减少生命周期提高周转
#define COMPUTATION_LIMIT 500    // 适中的计算量

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
    double total_response_time;   // 新增：总响应时间
    size_t total_tasks;          // 新增：总任务数
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
    
    // 减少处理间隔提高响应速度
    if (now - last_process_time < 2) return;  // 从5ms改为2ms
    last_process_time = now;
    
    size_t active_count = 0;
    
    // 直接遍历任务池
    for (size_t i = 0; i < 200; i++) {  // 更新遍历范围
        if (!task_pool[i]) continue;
        
        if (InfraxAsyncClass.is_done(task_pool[i])) {
            TaskContext* ctx = (TaskContext*)task_pool[i]->arg;
            
            if (task_pool[i]->state == INFRAX_ASYNC_FULFILLED) {
                metrics.completed_tasks++;
            } else {
                metrics.failed_tasks++;
            }
            
            cleanup_task(ctx);
            task_pool[i] = NULL;
        } else {
            active_count++;
            InfraxAsyncClass.pollset_poll(task_pool[i], 5);  // 减少poll间隔
            if (task_pool[i]->state == INFRAX_ASYNC_PENDING) {
                InfraxAsyncClass.start(task_pool[i]);
            }
        }
    }
    
    metrics.active_tasks = active_count;
}

// Create a batch of tasks with rate limiting
static void create_task_batch(size_t target_tasks) {
    InfraxTime now = core->time_now_ms(core);
    
    // 更积极的任务创建策略
    InfraxTime time_delta = now - metrics.last_batch_time;
    if (time_delta < 0) return;  // 只检查时间有效性
    
    // 更激进的批处理策略
    size_t max_new_tasks = (metrics.active_tasks >= TARGET_CONNECTIONS * 0.95) ? 
        BATCH_SIZE / 4 : // 高负载时仍保持一定创建量
        BATCH_SIZE; // 正常负载使用标准批次大小
    
    size_t created = 0;
    for (size_t i = 0; i < max_new_tasks && metrics.active_tasks < TARGET_CONNECTIONS; i++) {
        if (task_pool_index >= 200) {
            task_pool_index = 0;
        }
        
        if (task_pool[task_pool_index] != NULL) {
            task_pool_index++;
            continue;
        }
        
        TaskContext* ctx = memory->alloc(memory, sizeof(TaskContext));
        if (!ctx) break;
        
        core->memset(core, ctx, 0, sizeof(TaskContext));
        ctx->is_active = 1;
        ctx->state = 0;
        core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &ctx->start_time);
        
        InfraxAsync* async = InfraxAsyncClass.new(long_running_task, ctx);
        if (!async) {
            memory->dealloc(memory, ctx);
            break;
        }
        
        ctx->async = async;
        task_pool[task_pool_index] = async;
        
        if (!InfraxAsyncClass.start(async)) {
            cleanup_task(ctx);
            task_pool[task_pool_index] = NULL;
            continue;
        }
        
        created++;
        task_pool_index++;
        __atomic_fetch_add(&metrics.active_tasks, 1, __ATOMIC_SEQ_CST);
        if (metrics.active_tasks > metrics.peak_active_tasks) {
            metrics.peak_active_tasks = metrics.active_tasks;
        }
    }
    
    if (created > 0) {
        metrics.last_batch_time = now;
        metrics.current_batch_count += created;
        metrics.total_tasks += created;
    }
}

// Long running task function
void long_running_task(InfraxAsync* self, void* arg) {
    if (!self || !arg) return;

    TaskContext* ctx = (TaskContext*)arg;
    
    // First time entry
    if (ctx->state == 0) {
        ctx->state = 1;
        core->clock_gettime(core, INFRAX_CLOCK_MONOTONIC, &ctx->start_time);
        ctx->computation_count = 0;
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
        
        // 更新响应时间统计
        metrics.total_response_time += elapsed_ms;
        metrics.total_tasks++;
        metrics.avg_response_time = metrics.total_response_time / metrics.total_tasks;
        
        core->printf(core, "Task completed after %ld ms\n", elapsed_ms);
        return;
    }
    
    // 优化计算逻辑，减少不必要的计算
    if (ctx->computation_count < COMPUTATION_LIMIT) {
        // 每次增加更多计数，减少循环次数
        ctx->computation_count += 10;
        
        // 模拟一些轻量级计算
        for(volatile int i = 0; i < 10; i++) {
            volatile int x = i + 1;
        }
    } else {
        // 达到计算限制，完成任务
        self->state = INFRAX_ASYNC_FULFILLED;
        core->printf(core, "Task completed (computation limit)\n");
    }
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
    core = &gInfraxCore;//or InfraxCoreClass.singleton();
    
    core->printf(core, "Initializing core...\n");
    
    core->printf(core, "Initializing memory...\n");
    InfraxMemoryConfig config = {
        .initial_size = 64 * 1024 * 1024,  // 64MB initial size
        .use_gc = INFRAX_TRUE,
        .use_pool = INFRAX_TRUE,
        .gc_threshold = 32 * 1024 * 1024  // 32MB
    };
    memory = InfraxMemoryClass.new(&config);
    if (!memory) {
        core->printf(core, "Failed to initialize memory!\n");
        return 1;
    }
    
    core->printf(core, "Initialization completed, starting test...\n");
    
    // Initialize metrics
    core->memset(core, &metrics, 0, sizeof(TestMetrics));
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
        InfraxAsyncClass.pollset_poll(NULL, 10);
        core->sleep_ms(core,1);
    }
    
    // Print final metrics
    print_metrics(TEST_DURATION_SEC);
    
    // Cleanup
    core->printf(core, "\nTest completed. Cleaning up...\n");
    
    // 确保所有任务都被处理
    InfraxTime cleanup_start = core->time_monotonic_ms(core);
    while (1) {
        process_active_tasks();
        
        // 检查是否所有任务都已清理
        bool all_slots_empty = true;
        for (size_t i = 0; i < 200; i++) {
            if (task_pool[i] != NULL) {
                all_slots_empty = false;
                break;
            }
        }
        
        if (all_slots_empty) break;
        
        // 防止清理死循环
        InfraxTime current = core->time_monotonic_ms(core);
        if (current - cleanup_start > 5000) {  // 5秒超时保护
            core->printf(core, "Cleanup timeout, forcing exit...\n");
            break;
        }
        
        InfraxAsyncClass.pollset_poll(NULL, 10);
        core->sleep_ms(core,1);
    }
    
    if (memory) {
        InfraxMemoryClass.free(memory);
    }
    
    return 0;
}
