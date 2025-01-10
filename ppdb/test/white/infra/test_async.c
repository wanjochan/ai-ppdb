#include "test_common.h"
#include "test_framework.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_sync.h"

static int counter = 0;

static void test_callback(infra_async_task_t* task, infra_error_t error) {
    if (error == INFRA_OK && task && task->user_data) {
        int* counter_ptr = (int*)task->user_data;
        (*counter_ptr)++;
    }
}

static int test_async_loop(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_async_task_t task;

    err = infra_async_init(&ctx);
    ASSERT_OK(err);

    // 重置计数器
    counter = 0;

    // 提交一个简单的任务
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_EVENT;
    task.callback = test_callback;
    task.user_data = &counter;
    task.event.event_fd = -1;
    task.event.value = 1;

    err = infra_async_submit(ctx, &task);
    ASSERT_OK(err);

    err = infra_async_run(ctx, 1000);  // 1秒超时
    ASSERT_OK(err);
    TEST_ASSERT(counter == 1);  // 确保任务已完成

    infra_async_stop(ctx);
    infra_async_destroy(ctx);
    return 0;
}

static int test_async_task(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_async_task_t task;
    counter = 0;

    err = infra_async_init(&ctx);
    printf("infra_async_init returned: %d (%s)\n", err, infra_error_string(err));
    ASSERT_OK(err);

    // Submit task
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_EVENT;
    task.callback = test_callback;
    task.user_data = &counter;
    task.event.event_fd = -1;  // Special value for test task
    task.event.value = 1;

    err = infra_async_submit(ctx, &task);
    printf("infra_async_submit returned: %d (%s)\n", err, infra_error_string(err));
    ASSERT_OK(err);

    printf("Running async task...\n");
    err = infra_async_run(ctx, 1000);
    printf("infra_async_run returned: %d (%s)\n", err, infra_error_string(err));
    printf("counter = %d\n", counter);
    ASSERT_OK(err);
    TEST_ASSERT(counter == 1);

    // 先停止异步系统
    err = infra_async_stop(ctx);
    ASSERT_OK(err);

    infra_async_destroy(ctx);
    return 0;
}

static int test_async_cancel(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_async_task_t task;
    counter = 0;

    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);

    // Submit task
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_EVENT;
    task.callback = test_callback;
    task.user_data = &counter;
    task.event.event_fd = -1;  // Special value for test task
    task.event.value = 1;

    err = infra_async_submit(ctx, &task);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_cancel(ctx, &task);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(ctx, 1000);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(counter == 0);

    // 先停止异步系统
    err = infra_async_stop(ctx);
    TEST_ASSERT(err == INFRA_OK);

    infra_async_destroy(ctx);
    return 0;
}

static int test_async_io(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_async_task_t task;
    const char* data = "Hello, World!";
    size_t data_len = strlen(data);
    char buf[256];
    int fd;
    off_t size;

    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);

    fd = open("test.txt", O_CREAT | O_RDWR, 0644);
    TEST_ASSERT(fd != -1);

    // Write test
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_WRITE;
    task.callback = test_callback;
    task.user_data = &counter;
    task.io.fd = fd;
    task.io.buffer = (void*)data;
    task.io.size = data_len;

    err = infra_async_submit(ctx, &task);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(ctx, 1000);
    TEST_ASSERT(err == INFRA_OK);

    size = lseek(fd, 0, SEEK_END);
    TEST_ASSERT((size_t)size == data_len);

    lseek(fd, 0, SEEK_SET);

    // Read test
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_READ;
    task.callback = test_callback;
    task.user_data = &counter;
    task.io.fd = fd;
    task.io.buffer = buf;
    task.io.size = data_len;

    err = infra_async_submit(ctx, &task);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(ctx, 1000);
    TEST_ASSERT(err == INFRA_OK);

    TEST_ASSERT(memcmp(buf, data, data_len) == 0);

    // 先停止异步系统
    err = infra_async_stop(ctx);
    TEST_ASSERT(err == INFRA_OK);

    close(fd);
    unlink("test.txt");
    infra_async_destroy(ctx);
    return 0;
}

#define NUM_TASKS 100

static int test_async_performance(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_async_task_t task;
    counter = 0;

    printf("Starting performance test...\n");
    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);
    printf("Async context initialized\n");

    printf("Submitting %d tasks...\n", NUM_TASKS);
    for (int i = 0; i < NUM_TASKS; i++) {
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_EVENT;
        task.callback = test_callback;
        task.user_data = &counter;
        task.event.event_fd = -1;  // Special value for test task
        task.event.value = 1;

        err = infra_async_submit(ctx, &task);
        TEST_ASSERT(err == INFRA_OK);
    }
    printf("All tasks submitted\n");

    printf("Running tasks with 60s timeout...\n");
    err = infra_async_run(ctx, 60000);  // 增加超时时间到60秒
    TEST_ASSERT(err == INFRA_OK);
    printf("Tasks completed, counter = %d (expected %d)\n", counter, NUM_TASKS);
    TEST_ASSERT(counter == NUM_TASKS);

    // 获取统计信息
    infra_async_stats_t stats;
    err = infra_async_get_stats(ctx, &stats);
    TEST_ASSERT(err == INFRA_OK);
    printf("Stats: queued=%u, completed=%u, failed=%u, cancelled=%u\n",
           stats.queued_tasks, stats.completed_tasks, stats.failed_tasks, stats.cancelled_tasks);

    // 先停止异步系统
    printf("Stopping async system...\n");
    err = infra_async_stop(ctx);
    TEST_ASSERT(err == INFRA_OK);
    printf("Async system stopped\n");

    infra_async_destroy(ctx);
    return 0;
}

static int test_async_boundary_conditions(void) {
    infra_error_t err;
    infra_async_context_t* ctx;

    // Test NULL parameters
    err = infra_async_init(NULL);
    TEST_ASSERT(err == INFRA_ERROR_INVALID);

    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);

    // Test extreme timeout values
    err = infra_async_run(ctx, 0);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(ctx, UINT32_MAX);
    TEST_ASSERT(err == INFRA_OK);

    // 先停止异步系统
    err = infra_async_stop(ctx);
    TEST_ASSERT(err == INFRA_OK);

    infra_async_destroy(ctx);
    return 0;
}

#define NUM_THREADS 4
#define TASKS_PER_THREAD 250

typedef struct {
    infra_async_context_t* ctx;
    int num_tasks;
    infra_async_task_t* tasks;
} thread_data_t;

static void* concurrent_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    infra_error_t err;

    for (int i = 0; i < data->num_tasks; i++) {
        err = infra_async_submit(data->ctx, &data->tasks[i]);
        TEST_ASSERT(err == INFRA_OK);
    }

    return NULL;
}

static void* async_runner(void* arg) {
    infra_async_context_t* ctx = (infra_async_context_t*)arg;
    return (void*)(uintptr_t)infra_async_run(ctx, 5000);  // 5秒超时
}

static int test_async_concurrent(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_thread_t runner_thread;
    infra_thread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    infra_async_task_t tasks[NUM_THREADS * TASKS_PER_THREAD];
    int counter = 0;

    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);
    printf("Async context initialized\n");

    // 初始化任务
    for (int i = 0; i < NUM_THREADS; i++) {
        for (int j = 0; j < TASKS_PER_THREAD; j++) {
            tasks[i * TASKS_PER_THREAD + j].type = INFRA_ASYNC_EVENT;
            tasks[i * TASKS_PER_THREAD + j].callback = test_callback;
            tasks[i * TASKS_PER_THREAD + j].user_data = &counter;
            tasks[i * TASKS_PER_THREAD + j].event.event_fd = -1;
            tasks[i * TASKS_PER_THREAD + j].event.value = 1;
        }
    }
    printf("Tasks initialized: %d total\n", NUM_THREADS * TASKS_PER_THREAD);

    // 创建运行器线程
    err = infra_thread_create((void**)&runner_thread, async_runner, ctx);
    TEST_ASSERT(err == INFRA_OK);
    printf("Runner thread created\n");

    // 创建工作线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].ctx = ctx;
        thread_data[i].num_tasks = TASKS_PER_THREAD;
        thread_data[i].tasks = &tasks[i * TASKS_PER_THREAD];
        err = infra_thread_create((void**)&threads[i], concurrent_worker, &thread_data[i]);
        TEST_ASSERT(err == INFRA_OK);
    }
    printf("Worker threads created: %d\n", NUM_THREADS);

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        err = infra_thread_join(threads[i]);
        TEST_ASSERT(err == INFRA_OK);
    }
    printf("All worker threads joined\n");

    err = infra_thread_join(runner_thread);
    TEST_ASSERT(err == INFRA_OK);
    printf("Runner thread joined\n");

    // 获取统计信息
    infra_async_stats_t stats;
    err = infra_async_get_stats(ctx, &stats);
    TEST_ASSERT(err == INFRA_OK);
    printf("Stats: queued=%u, completed=%u, failed=%u, cancelled=%u\n",
           stats.queued_tasks, stats.completed_tasks, stats.failed_tasks, stats.cancelled_tasks);

    // 验证结果
    printf("Final counter value: %d (expected: %d)\n", counter, NUM_THREADS * TASKS_PER_THREAD);
    TEST_ASSERT(counter == NUM_THREADS * TASKS_PER_THREAD);

    infra_async_destroy(ctx);
    return 0;
}

#define CPU_TASK_DELAY_MS 20  // CPU密集型任务延迟
#define IO_TASK_SIZE 4096    // IO任务数据大小

// CPU密集型任务模拟函数
static void cpu_intensive_work(void) {
    // 模拟CPU密集型计算
    volatile double result = 0.0;
    for (int i = 0; i < 1000000; i++) {
        result += sqrt((double)i);
    }
}

// IO密集型任务回调
static void io_task_callback(infra_async_task_t* task, infra_error_t error) {
    if (error == INFRA_OK && task && task->user_data) {
        int* counter_ptr = (int*)task->user_data;
        (*counter_ptr)++;
        
        // 验证任务分类
        TEST_ASSERT(task->profile.type == INFRA_TASK_TYPE_IO);
        TEST_ASSERT(task->profile.process_method == INFRA_PROCESS_EVENTFD);
    }
}

// CPU密集型任务回调
static void cpu_task_callback(infra_async_task_t* task, infra_error_t error) {
    if (error == INFRA_OK && task && task->user_data) {
        int* counter_ptr = (int*)task->user_data;
        (*counter_ptr)++;
        
        // 验证任务分类
        TEST_ASSERT(task->profile.type == INFRA_TASK_TYPE_CPU);
        TEST_ASSERT(task->profile.process_method == INFRA_PROCESS_THREAD);
    }
}

static int test_async_task_classification(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_async_task_t task;
    int io_counter = 0;
    int cpu_counter = 0;
    char* test_file = "test_async_perf.dat";
    char buffer[IO_TASK_SIZE];
    
    printf("Starting task classification test...\n");
    
    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建测试文件
    int fd = open(test_file, O_CREAT | O_RDWR, 0644);
    TEST_ASSERT(fd != -1);
    
    // 提交IO密集型任务
    printf("Submitting IO intensive tasks...\n");
    for (int i = 0; i < 50; i++) {
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_WRITE;
        task.callback = io_task_callback;
        task.user_data = &io_counter;
        task.io.fd = fd;
        task.io.buffer = buffer;
        task.io.size = IO_TASK_SIZE;
        
        err = infra_async_submit(ctx, &task);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 提交CPU密集型任务
    printf("Submitting CPU intensive tasks...\n");
    for (int i = 0; i < 50; i++) {
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_EVENT;
        task.callback = cpu_task_callback;
        task.user_data = &cpu_counter;
        task.event.event_fd = -1;
        task.event.value = 1;
        
        err = infra_async_submit(ctx, &task);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 运行并等待所有任务完成
    printf("Running tasks...\n");
    err = infra_async_run(ctx, 30000);  // 30秒超时
    TEST_ASSERT(err == INFRA_OK);
    
    // 获取统计信息
    infra_async_stats_t stats;
    err = infra_async_get_stats(ctx, &stats);
    TEST_ASSERT(err == INFRA_OK);
    
    printf("Task Statistics:\n");
    printf("- Queued: %u\n", stats.queued_tasks);
    printf("- Completed: %u\n", stats.completed_tasks);
    printf("- Failed: %u\n", stats.failed_tasks);
    printf("- Average Process Time: %lu us\n", 
           stats.completed_tasks > 0 ? stats.total_process_time_us / stats.completed_tasks : 0);
    printf("- Max Process Time: %lu us\n", stats.max_process_time_us);
    
    // 验证计数器
    printf("IO Tasks Completed: %d\n", io_counter);
    printf("CPU Tasks Completed: %d\n", cpu_counter);
    TEST_ASSERT(io_counter == 50);
    TEST_ASSERT(cpu_counter == 50);
    
    // 清理
    close(fd);
    unlink(test_file);
    infra_async_destroy(ctx);
    
    return 0;
}

static int test_async_mixed_workload(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_async_task_t task;
    int io_counter = 0;
    int cpu_counter = 0;
    char* test_file = "test_mixed_workload.dat";
    char buffer[IO_TASK_SIZE];
    infra_time_t start_time, end_time;
    
    printf("Starting mixed workload test...\n");
    
    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建测试文件
    int fd = open(test_file, O_CREAT | O_RDWR, 0644);
    TEST_ASSERT(fd != -1);
    
    start_time = infra_time_monotonic();
    
    // 交替提交IO和CPU任务
    printf("Submitting mixed workload...\n");
    for (int i = 0; i < 100; i++) {
        // IO任务
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_WRITE;
        task.callback = io_task_callback;
        task.user_data = &io_counter;
        task.io.fd = fd;
        task.io.buffer = buffer;
        task.io.size = IO_TASK_SIZE;
        
        err = infra_async_submit(ctx, &task);
        TEST_ASSERT(err == INFRA_OK);
        
        // CPU任务
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_EVENT;
        task.callback = cpu_task_callback;
        task.user_data = &cpu_counter;
        task.event.event_fd = -1;
        task.event.value = 1;
        
        err = infra_async_submit(ctx, &task);
        TEST_ASSERT(err == INFRA_OK);
    }
    
    // 运行并等待所有任务完成
    printf("Running mixed workload...\n");
    err = infra_async_run(ctx, 60000);  // 60秒超时
    TEST_ASSERT(err == INFRA_OK);
    
    end_time = infra_time_monotonic();
    
    // 获取统计信息
    infra_async_stats_t stats;
    err = infra_async_get_stats(ctx, &stats);
    TEST_ASSERT(err == INFRA_OK);
    
    printf("Mixed Workload Statistics:\n");
    printf("- Total Time: %lu us\n", end_time - start_time);
    printf("- Queued: %u\n", stats.queued_tasks);
    printf("- Completed: %u\n", stats.completed_tasks);
    printf("- Failed: %u\n", stats.failed_tasks);
    printf("- Average Process Time: %lu us\n", 
           stats.completed_tasks > 0 ? stats.total_process_time_us / stats.completed_tasks : 0);
    printf("- Max Process Time: %lu us\n", stats.max_process_time_us);
    
    // 验证计数器
    printf("IO Tasks Completed: %d\n", io_counter);
    printf("CPU Tasks Completed: %d\n", cpu_counter);
    TEST_ASSERT(io_counter == 100);
    TEST_ASSERT(cpu_counter == 100);
    
    // 清理
    close(fd);
    unlink(test_file);
    infra_async_destroy(ctx);
    
    return 0;
}

int main(void) {
    TEST_INIT();
    printf("Async System Tests\n");
    
    TEST_RUN(test_async_loop);
    TEST_RUN(test_async_task);
    TEST_RUN(test_async_cancel);
    TEST_RUN(test_async_io);
    TEST_RUN(test_async_performance);
    TEST_RUN(test_async_boundary_conditions);
    TEST_RUN(test_async_concurrent);
    TEST_RUN(test_async_task_classification);  // 新增
    TEST_RUN(test_async_mixed_workload);       // 新增
    
    TEST_CLEANUP();
    return test_stats.failed_tests;
} 