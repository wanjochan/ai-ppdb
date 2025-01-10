#include "test_common.h"
#include "test_framework.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_sync.h"

static int counter = 0;

static void test_callback(infra_async_task_t* task, infra_error_t error) {
    if (error == INFRA_OK && task) {
        counter++;
    }
}

static int test_async_loop(void) {
    infra_error_t err;
    infra_async_t async;
    infra_async_task_t task;
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    err = infra_async_init(&async, &config);
    ASSERT_OK(err);

    // 重置计数器
    counter = 0;

    // 提交一个简单的任务
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_EVENT;
    task.callback = test_callback;

    err = infra_async_submit(&async, &task);
    ASSERT_OK(err);

    err = infra_async_run(&async, 1000);  // 1秒超时
    ASSERT_OK(err);
    TEST_ASSERT(counter == 1);  // 确保任务已完成

    infra_async_stop(&async);
    infra_async_destroy(&async);
    return 0;
}

static int test_async_task(void) {
    infra_error_t err;
    infra_async_t async;
    infra_async_task_t task;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    counter = 0;

    err = infra_async_init(&async, &config);
    ASSERT_OK(err);

    // Submit task
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_EVENT;
    task.callback = test_callback;

    err = infra_async_submit(&async, &task);
    ASSERT_OK(err);

    printf("Running async task...\n");
    err = infra_async_run(&async, 1000);
    ASSERT_OK(err);
    TEST_ASSERT(counter == 1);

    // 先停止异步系统
    err = infra_async_stop(&async);
    ASSERT_OK(err);

    infra_async_destroy(&async);
    return 0;
}

static int test_async_cancel(void) {
    infra_error_t err;
    infra_async_t async;
    infra_async_task_t task;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    counter = 0;

    err = infra_async_init(&async, &config);
    TEST_ASSERT(err == INFRA_OK);

    // Submit task
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_EVENT;
    task.callback = test_callback;

    err = infra_async_submit(&async, &task);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_cancel(&async, &task);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(&async, 1000);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(counter == 0);

    // 先停止异步系统
    err = infra_async_stop(&async);
    TEST_ASSERT(err == INFRA_OK);

    infra_async_destroy(&async);
    return 0;
}

static int test_async_io(void) {
    infra_error_t err;
    infra_async_t async;
    infra_async_task_t task;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    const char* data = "Hello, World!";
    size_t data_len = infra_strlen(data);
    char buf[256];
    infra_handle_t fd;

    err = infra_async_init(&async, &config);
    TEST_ASSERT(err == INFRA_OK);

    // 使用Infra层的文件操作函数
    err = infra_file_open("test.txt", INFRA_FILE_CREATE | INFRA_FILE_RDWR, 0644, &fd);
    TEST_ASSERT(err == INFRA_OK);

    // Write test
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_WRITE;
    task.callback = test_callback;
    task.io.fd = fd;
    task.io.buffer = (void*)data;
    task.io.size = data_len;

    err = infra_async_submit(&async, &task);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(&async, 1000);
    TEST_ASSERT(err == INFRA_OK);

    // 验证文件大小
    size_t file_size;
    err = infra_file_size(fd, &file_size);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(file_size == data_len);

    // 重置文件位置
    err = infra_file_seek(fd, 0, INFRA_SEEK_SET);
    TEST_ASSERT(err == INFRA_OK);

    // Read test
    memset(&task, 0, sizeof(task));
    task.type = INFRA_ASYNC_READ;
    task.callback = test_callback;
    task.io.fd = fd;
    task.io.buffer = buf;
    task.io.size = data_len;

    err = infra_async_submit(&async, &task);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(&async, 1000);
    TEST_ASSERT(err == INFRA_OK);

    TEST_ASSERT(infra_memcmp(buf, data, data_len) == 0);

    // 先停止异步系统
    err = infra_async_stop(&async);
    TEST_ASSERT(err == INFRA_OK);

    // 清理资源
    err = infra_file_close(fd);
    TEST_ASSERT(err == INFRA_OK);
    err = infra_file_remove("test.txt");
    TEST_ASSERT(err == INFRA_OK);

    infra_async_destroy(&async);
    return 0;
}

#define NUM_TASKS 100

static int test_async_performance(void) {
    infra_error_t err;
    infra_async_t async;
    infra_async_task_t task;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    counter = 0;

    printf("Starting performance test...\n");
    err = infra_async_init(&async, &config);
    TEST_ASSERT(err == INFRA_OK);
    printf("Async context initialized\n");

    printf("Submitting %d tasks...\n", NUM_TASKS);
    for (int i = 0; i < NUM_TASKS; i++) {
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_EVENT;
        task.callback = test_callback;

        err = infra_async_submit(&async, &task);
        TEST_ASSERT(err == INFRA_OK);
    }
    printf("All tasks submitted\n");

    printf("Running tasks with 60s timeout...\n");
    err = infra_async_run(&async, 60000);  // 增加超时时间到60秒
    TEST_ASSERT(err == INFRA_OK);
    printf("Tasks completed, counter = %d (expected %d)\n", counter, NUM_TASKS);
    TEST_ASSERT(counter == NUM_TASKS);

    // 获取统计信息
    infra_async_stats_t stats;
    err = infra_async_get_stats(&async, &stats);
    TEST_ASSERT(err == INFRA_OK);
    printf("Stats: queued=%u, completed=%u, failed=%u, cancelled=%u\n",
           stats.queued_tasks, stats.completed_tasks, stats.failed_tasks, stats.cancelled_tasks);

    // 先停止异步系统
    printf("Stopping async system...\n");
    err = infra_async_stop(&async);
    TEST_ASSERT(err == INFRA_OK);
    printf("Async system stopped\n");

    infra_async_destroy(&async);
    return 0;
}

static int test_async_boundary_conditions(void) {
    infra_error_t err;
    infra_async_t async;
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    // Test NULL parameters
    err = infra_async_init(NULL, &config);
    TEST_ASSERT(err == INFRA_ERROR_INVALID);

    err = infra_async_init(&async, &config);
    TEST_ASSERT(err == INFRA_OK);

    // Test extreme timeout values
    err = infra_async_run(&async, 0);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(&async, UINT32_MAX);
    TEST_ASSERT(err == INFRA_OK);

    // 先停止异步系统
    err = infra_async_stop(&async);
    TEST_ASSERT(err == INFRA_OK);

    infra_async_destroy(&async);
    return 0;
}

typedef struct {
    infra_async_t* async;
    int num_tasks;
    infra_async_task_t* tasks;
} thread_data_t;

static void* concurrent_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    infra_error_t err;

    for (int i = 0; i < data->num_tasks; i++) {
        err = infra_async_submit(data->async, &data->tasks[i]);
        if (err != INFRA_OK) {
            return (void*)(uintptr_t)err;
        }
    }

    return NULL;
}

static void* async_runner(void* arg) {
    infra_async_t* async = (infra_async_t*)arg;
    return (void*)(uintptr_t)infra_async_run(async, 5000);  // 5秒超时
}

#define NUM_THREADS 4
#define TASKS_PER_THREAD 25

static int test_async_concurrent(void) {
    infra_error_t err;
    infra_async_t async;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_thread_t threads[NUM_THREADS];
    infra_thread_t runner_thread;
    thread_data_t thread_data[NUM_THREADS];
    infra_async_task_t tasks[NUM_THREADS * TASKS_PER_THREAD];
    counter = 0;

    err = infra_async_init(&async, &config);
    TEST_ASSERT(err == INFRA_OK);

    // 初始化任务
    for (int i = 0; i < NUM_THREADS; i++) {
        for (int j = 0; j < TASKS_PER_THREAD; j++) {
            memset(&tasks[i * TASKS_PER_THREAD + j], 0, sizeof(infra_async_task_t));
            tasks[i * TASKS_PER_THREAD + j].type = INFRA_ASYNC_EVENT;
            tasks[i * TASKS_PER_THREAD + j].callback = test_callback;
        }

        thread_data[i].async = &async;
        thread_data[i].num_tasks = TASKS_PER_THREAD;
        thread_data[i].tasks = &tasks[i * TASKS_PER_THREAD];
    }

    // 创建运行线程
    err = infra_thread_create(&runner_thread, async_runner, &async);
    TEST_ASSERT(err == INFRA_OK);

    // 创建工作线程
    for (int i = 0; i < NUM_THREADS; i++) {
        err = infra_thread_create(&threads[i], concurrent_worker, &thread_data[i]);
        TEST_ASSERT(err == INFRA_OK);
    }

    // 等待工作线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        err = infra_thread_join(threads[i]);
        TEST_ASSERT(err == INFRA_OK);
    }

    // 等待运行线程完成
    err = infra_thread_join(runner_thread);
    TEST_ASSERT(err == INFRA_OK);

    // 获取统计信息
    infra_async_stats_t stats;
    err = infra_async_get_stats(&async, &stats);
    TEST_ASSERT(err == INFRA_OK);
    printf("Stats: queued=%u, completed=%u, failed=%u, cancelled=%u\n",
           stats.queued_tasks, stats.completed_tasks, stats.failed_tasks, stats.cancelled_tasks);

    // 验证结果
    TEST_ASSERT(counter == NUM_THREADS * TASKS_PER_THREAD);

    infra_async_destroy(&async);
    return 0;
}

static void cpu_intensive_work(void) {
    volatile int x = 0;
    for (int i = 0; i < 1000000; i++) {
        x += i;
    }
}

static void io_task_callback(infra_async_task_t* task, infra_error_t error) {
    if (error == INFRA_OK && task) {
        counter++;
    }
}

static void cpu_task_callback(infra_async_task_t* task, infra_error_t error) {
    if (error == INFRA_OK && task) {
        cpu_intensive_work();
        counter++;
    }
}

static int test_async_task_classification(void) {
    infra_error_t err;
    infra_async_t async;
    infra_async_task_t task;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    int io_counter = 0;
    int cpu_counter = 0;

    // 配置任务分类参数
    config.async.classify.io_threshold_us = 1000;   // 1ms
    config.async.classify.cpu_threshold_us = 10000; // 10ms

    err = infra_async_init(&async, &config);
    TEST_ASSERT(err == INFRA_OK);

    // 提交IO密集型任务
    for (int i = 0; i < 50; i++) {
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_READ;
        task.callback = io_task_callback;
        task.io.fd = -1;  // 模拟IO操作
        task.io.buffer = NULL;
        task.io.size = 0;

        err = infra_async_submit(&async, &task);
        TEST_ASSERT(err == INFRA_OK);
    }

    // 提交CPU密集型任务
    for (int i = 0; i < 50; i++) {
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_EVENT;
        task.callback = cpu_task_callback;

        err = infra_async_submit(&async, &task);
        TEST_ASSERT(err == INFRA_OK);
    }

    // 运行任务
    err = infra_async_run(&async, 30000);  // 30秒超时
    TEST_ASSERT(err == INFRA_OK);

    // 获取统计信息
    infra_async_stats_t stats;
    err = infra_async_get_stats(&async, &stats);
    TEST_ASSERT(err == INFRA_OK);
    printf("Stats: queued=%u, completed=%u, failed=%u, cancelled=%u\n",
           stats.queued_tasks, stats.completed_tasks, stats.failed_tasks, stats.cancelled_tasks);

    // 验证结果
    TEST_ASSERT(io_counter + cpu_counter == 100);
    printf("IO tasks: %d, CPU tasks: %d\n", io_counter, cpu_counter);

    // 先停止异步系统
    err = infra_async_stop(&async);
    TEST_ASSERT(err == INFRA_OK);

    infra_async_destroy(&async);
    return 0;
}

static int test_async_mixed_workload(void) {
    infra_error_t err;
    infra_async_t async;
    infra_async_task_t task;
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    int io_counter = 0;
    int cpu_counter = 0;

    // 配置任务分类参数
    config.async.classify.io_threshold_us = 1000;   // 1ms
    config.async.classify.cpu_threshold_us = 10000; // 10ms

    err = infra_async_init(&async, &config);
    TEST_ASSERT(err == INFRA_OK);

    // 交替提交IO和CPU密集型任务
    for (int i = 0; i < 100; i++) {
        memset(&task, 0, sizeof(task));

        if (i % 2 == 0) {
            // IO任务
            task.type = INFRA_ASYNC_READ;
            task.callback = io_task_callback;
            task.io.fd = -1;  // 模拟IO操作
            task.io.buffer = NULL;
            task.io.size = 0;

            err = infra_async_submit(&async, &task);
            TEST_ASSERT(err == INFRA_OK);
        } else {
            // CPU任务
            task.type = INFRA_ASYNC_EVENT;
            task.callback = cpu_task_callback;

            err = infra_async_submit(&async, &task);
            TEST_ASSERT(err == INFRA_OK);
        }
    }

    // 运行任务
    err = infra_async_run(&async, 60000);  // 60秒超时
    TEST_ASSERT(err == INFRA_OK);

    // 获取统计信息
    infra_async_stats_t stats;
    err = infra_async_get_stats(&async, &stats);
    TEST_ASSERT(err == INFRA_OK);
    printf("Stats: queued=%u, completed=%u, failed=%u, cancelled=%u\n",
           stats.queued_tasks, stats.completed_tasks, stats.failed_tasks, stats.cancelled_tasks);

    // 验证结果
    TEST_ASSERT(io_counter + cpu_counter == 100);
    printf("IO tasks: %d, CPU tasks: %d\n", io_counter, cpu_counter);

    // 验证任务分类
    TEST_ASSERT(io_counter >= 45 && io_counter <= 55);  // 允许一定的误差
    TEST_ASSERT(cpu_counter >= 45 && cpu_counter <= 55);

    // 先停止异步系统
    err = infra_async_stop(&async);
    TEST_ASSERT(err == INFRA_OK);

    infra_async_destroy(&async);
    return 0;
}

int main(void) {
    TEST_INIT();

    TEST_RUN(test_async_loop);
    TEST_RUN(test_async_task);
    TEST_RUN(test_async_cancel);
    TEST_RUN(test_async_io);
    TEST_RUN(test_async_performance);
    TEST_RUN(test_async_boundary_conditions);
    TEST_RUN(test_async_concurrent);
    TEST_RUN(test_async_task_classification);
    TEST_RUN(test_async_mixed_workload);

    TEST_CLEANUP();
    return test_stats.failed_tests;
} 