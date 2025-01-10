#include "test_common.h"
#include "test_framework.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_sync.h"

static int counter = 0;

static void test_callback(infra_async_task_t* task, infra_error_t error) {
    if (error == INFRA_OK) {
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

    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);

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

    err = infra_async_run(ctx, 30000);  // 增加超时时间到30秒
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(counter == NUM_TASKS);

    // 先停止异步系统
    err = infra_async_stop(ctx);
    TEST_ASSERT(err == INFRA_OK);

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
    infra_thread_t* threads[NUM_THREADS];
    infra_thread_t* runner_thread;
    thread_data_t thread_data[NUM_THREADS];
    counter = 0;

    err = infra_async_init(&ctx);
    ASSERT_OK(err);

    // 先创建任务
    infra_async_task_t tasks[NUM_THREADS * TASKS_PER_THREAD];
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].ctx = ctx;
        thread_data[i].num_tasks = TASKS_PER_THREAD;
        thread_data[i].tasks = &tasks[i * TASKS_PER_THREAD];
        
        for (int j = 0; j < TASKS_PER_THREAD; j++) {
            memset(&tasks[i * TASKS_PER_THREAD + j], 0, sizeof(infra_async_task_t));
            tasks[i * TASKS_PER_THREAD + j].type = INFRA_ASYNC_EVENT;
            tasks[i * TASKS_PER_THREAD + j].callback = test_callback;
            tasks[i * TASKS_PER_THREAD + j].user_data = &counter;
            tasks[i * TASKS_PER_THREAD + j].event.event_fd = -1;
            tasks[i * TASKS_PER_THREAD + j].event.value = 1;
        }
    }

    // 启动异步处理线程
    err = infra_thread_create(&runner_thread, async_runner, ctx);
    ASSERT_OK(err);

    // 启动任务提交线程
    for (int i = 0; i < NUM_THREADS; i++) {
        err = infra_thread_create(&threads[i], concurrent_worker, &thread_data[i]);
        ASSERT_OK(err);
    }

    // 等待所有提交线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        err = infra_thread_join(threads[i]);
        ASSERT_OK(err);
    }

    // 停止异步处理
    infra_async_stop(ctx);

    // 等待异步处理线程完成
    err = infra_thread_join(runner_thread);
    ASSERT_OK(err);

    // 验证所有任务都已完成
    TEST_ASSERT(counter == NUM_THREADS * TASKS_PER_THREAD);

    infra_async_destroy(ctx);
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

    TEST_CLEANUP();
    return test_stats.failed_tests ? 1 : 0;
} 