#include "test_common.h"
#include "test_framework.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_sync.h"

static int counter = 0;

static void test_callback(infra_async_task_t* task, infra_error_t error) {
    (void)error;
    int* counter_ptr = (int*)task->user_data;
    (*counter_ptr)++;
}

static void test_async_loop(void) {
    infra_error_t err;
    infra_async_context_t* ctx;

    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);

    err = infra_async_run(ctx, 0);  // Non-blocking
    TEST_ASSERT(err == INFRA_OK);

    infra_async_stop(ctx);
    infra_async_destroy(ctx);
}

static void test_async_task(void) {
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

    err = infra_async_run(ctx, 1000);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(counter == 1);

    infra_async_destroy(ctx);
}

static void test_async_cancel(void) {
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

    infra_async_destroy(ctx);
}

static void test_async_io(void) {
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

    close(fd);
    unlink("test.txt");
    infra_async_destroy(ctx);
}

#define NUM_TASKS 1000
static void test_async_performance(void) {
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

    err = infra_async_run(ctx, 10000);
    TEST_ASSERT(err == INFRA_OK);

    TEST_ASSERT(counter == NUM_TASKS);

    infra_async_destroy(ctx);
}

static void test_async_boundary_conditions(void) {
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

    infra_async_destroy(ctx);
}

#define NUM_THREADS 4
#define TASKS_PER_THREAD 250

typedef struct {
    infra_async_context_t* ctx;
    int num_tasks;
} thread_data_t;

static void concurrent_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    infra_error_t err;
    infra_async_task_t task;
    
    for (int i = 0; i < data->num_tasks; i++) {
        memset(&task, 0, sizeof(task));
        task.type = INFRA_ASYNC_EVENT;
        task.callback = test_callback;
        task.user_data = &counter;
        task.event.event_fd = -1;  // Special value for test task
        task.event.value = 1;

        err = infra_async_submit(data->ctx, &task);
        TEST_ASSERT(err == INFRA_OK);
    }
}

static void test_async_concurrent(void) {
    infra_error_t err;
    infra_async_context_t* ctx;
    infra_thread_t* threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    counter = 0;

    err = infra_async_init(&ctx);
    TEST_ASSERT(err == INFRA_OK);

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].ctx = ctx;
        thread_data[i].num_tasks = TASKS_PER_THREAD;
        err = infra_thread_create(&threads[i], concurrent_worker, &thread_data[i]);
        TEST_ASSERT(err == INFRA_OK);
    }

    err = infra_async_run(ctx, 0);  // Non-blocking
    TEST_ASSERT(err == INFRA_OK);

    for (int i = 0; i < NUM_THREADS; i++) {
        err = infra_thread_join(threads[i]);
        TEST_ASSERT(err == INFRA_OK);
    }

    TEST_ASSERT(counter == NUM_THREADS * TASKS_PER_THREAD);

    infra_async_destroy(ctx);
}

int main(void) {
    TEST_INIT();

    test_async_loop();
    test_async_task();
    test_async_cancel();
    test_async_io();
    test_async_performance();
    test_async_boundary_conditions();
    test_async_concurrent();

    TEST_SUMMARY();
    return TEST_RESULT();
} 