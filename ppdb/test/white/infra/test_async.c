#include "test/test_common.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_async.h"
#include "test/test_framework.h"

// Performance measurement utilities
#define PERF_START(start) start = ppdb_time_now()
#define PERF_END(start, end) do { \
    end = ppdb_time_now(); \
    double duration = (end - start) / 1000000.0; \
    printf("Operation took %.6f seconds\n", duration); \
} while(0)

static void test_callback(ppdb_error_t error, void* arg) {
    int* counter = (int*)arg;
    if (error == PPDB_OK) {
        (*counter)++;
    }
}

static void test_func(void* arg) {
    int* counter = (int*)arg;
    (*counter)++;
}

static int test_async_loop(void) {
    ppdb_async_loop_t* loop;
    ppdb_error_t err;

    // Test loop creation
    err = ppdb_async_loop_create(&loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop creation failed");

    // Test loop stop
    err = ppdb_async_loop_stop(loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop stop failed");

    // Test loop destroy
    err = ppdb_async_loop_destroy(loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop destroy failed");

    return 0;
}

static int test_async_task(void) {
    ppdb_async_loop_t* loop;
    ppdb_async_handle_t* handle;
    int counter = 0;
    ppdb_error_t err;

    // Create loop
    err = ppdb_async_loop_create(&loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop creation failed");

    // Submit task
    err = ppdb_async_submit(loop, test_func, &counter, 0, 0,
                           test_callback, &counter, &handle);
    TEST_ASSERT(err == PPDB_OK, "Task submission failed");

    // Run loop
    err = ppdb_async_loop_run(loop, 1000);
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");

    // Check results
    TEST_ASSERT(counter == 2, "Task execution failed");

    // Cleanup
    err = ppdb_async_loop_destroy(loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop destroy failed");

    return 0;
}

static int test_async_cancel(void) {
    ppdb_async_loop_t* loop;
    ppdb_async_handle_t* handle;
    int counter = 0;
    ppdb_error_t err;

    // Create loop
    err = ppdb_async_loop_create(&loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop creation failed");

    // Submit task
    err = ppdb_async_submit(loop, test_func, &counter, 0, 0,
                           test_callback, &counter, &handle);
    TEST_ASSERT(err == PPDB_OK, "Task submission failed");

    // Cancel task
    err = ppdb_async_cancel(handle);
    TEST_ASSERT(err == PPDB_OK, "Task cancellation failed");

    // Run loop
    err = ppdb_async_loop_run(loop, 1000);
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");

    // Check results
    TEST_ASSERT(counter == 0, "Cancelled task should not execute");

    // Cleanup
    err = ppdb_async_loop_destroy(loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop destroy failed");

    return 0;
}

static int test_async_io(void) {
    ppdb_async_loop_t* loop;
    int counter = 0;
    char buf[128] = {0};  // Initialize buffer
    ppdb_error_t err;
    int fd;

    // Create loop
    err = ppdb_async_loop_create(&loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop creation failed");

    // Create test file
    fd = open("test.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "File creation failed");

    // Write test
    const char* test_data = "Hello, World!";
    size_t data_len = strlen(test_data);

    // Write with offset 0
    err = ppdb_async_write(loop, fd, test_data, data_len, 0,
                          test_callback, &counter);
    TEST_ASSERT(err == PPDB_OK, "Async write failed");

    // Run loop with longer timeout
    err = ppdb_async_loop_run(loop, 5000);  // 5 seconds timeout
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");
    TEST_ASSERT(counter == 1, "Write callback not called");

    // Verify file size
    off_t size = lseek(fd, 0, SEEK_END);
    TEST_ASSERT(size == data_len, "File size mismatch");

    // Reset file position
    lseek(fd, 0, SEEK_SET);

    // Read test
    counter = 0;  // Reset counter
    err = ppdb_async_read(loop, fd, buf, data_len, 0,
                         test_callback, &counter);
    TEST_ASSERT(err == PPDB_OK, "Async read failed");

    // Run loop
    err = ppdb_async_loop_run(loop, 5000);  // 5 seconds timeout
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");
    TEST_ASSERT(counter == 1, "Read callback not called");

    // Compare data
    TEST_ASSERT(memcmp(buf, test_data, data_len) == 0,
                "Read data does not match written data");

    // Fsync test
    counter = 0;  // Reset counter
    err = ppdb_async_fsync(loop, fd, test_callback, &counter);
    TEST_ASSERT(err == PPDB_OK, "Async fsync failed");

    // Run loop
    err = ppdb_async_loop_run(loop, 5000);  // 5 seconds timeout
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");
    TEST_ASSERT(counter == 1, "Fsync callback not called");

    // Cleanup
    close(fd);
    unlink("test.txt");
    err = ppdb_async_loop_destroy(loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop destroy failed");

    return 0;
}

/* Performance Test */
static int test_async_performance(void) {
    ppdb_async_loop_t* loop;
    int64_t start, end;
    const int NUM_TASKS = 10000;
    int counter = 0;
    ppdb_error_t err;

    err = ppdb_async_loop_create(&loop);
    TEST_ASSERT(err == PPDB_OK, "Loop creation failed");

    PERF_START(start);
    for (int i = 0; i < NUM_TASKS; i++) {
        err = ppdb_async_submit(loop, test_func, &counter, 0, 0,
                              test_callback, &counter, NULL);
        TEST_ASSERT(err == PPDB_OK, "Task submission failed");
    }

    err = ppdb_async_loop_run(loop, 10000);
    PERF_END(start, end);

    TEST_ASSERT(counter == NUM_TASKS * 2, "Not all tasks completed");
    
    ppdb_async_loop_destroy(loop);
    return 0;
}

/* Boundary Conditions Test */
static int test_async_boundary_conditions(void) {
    ppdb_async_loop_t* loop;
    ppdb_error_t err;

    // Test NULL parameters
    err = ppdb_async_loop_create(NULL);
    TEST_ASSERT(err == PPDB_ERROR_INVALID_ARGUMENT, "NULL check failed");

    err = ppdb_async_loop_create(&loop);
    TEST_ASSERT(err == PPDB_OK, "Loop creation failed");

    // Test extreme timeout values
    err = ppdb_async_loop_run(loop, 0);
    TEST_ASSERT(err == PPDB_OK, "Zero timeout failed");

    err = ppdb_async_loop_run(loop, UINT32_MAX);
    TEST_ASSERT(err == PPDB_OK, "Maximum timeout failed");

    ppdb_async_loop_destroy(loop);
    return 0;
}

/* Concurrent Test */
#define NUM_THREADS 4
#define TASKS_PER_THREAD 1000

struct thread_data {
    ppdb_async_loop_t* loop;
    int counter;
};

static void* concurrent_worker(void* arg) {
    struct thread_data* data = (struct thread_data*)arg;
    
    for (int i = 0; i < TASKS_PER_THREAD; i++) {
        ppdb_async_submit(data->loop, test_func, &data->counter, 
                         0, 0, test_callback, &data->counter, NULL);
    }
    return NULL;
}

static int test_async_concurrent(void) {
    ppdb_async_loop_t* loop;
    ppdb_thread_t* threads[NUM_THREADS];
    struct thread_data thread_data[NUM_THREADS];
    ppdb_error_t err;

    err = ppdb_async_loop_create(&loop);
    TEST_ASSERT(err == PPDB_OK, "Loop creation failed");

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].loop = loop;
        thread_data[i].counter = 0;
        err = ppdb_thread_create(&threads[i], concurrent_worker, &thread_data[i]);
        TEST_ASSERT(err == PPDB_OK, "Thread creation failed");
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        err = ppdb_thread_join(threads[i]);
        TEST_ASSERT(err == PPDB_OK, "Thread join failed");
    }

    err = ppdb_async_loop_run(loop, 10000);
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");

    int total_counter = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        total_counter += thread_data[i].counter;
    }
    TEST_ASSERT(total_counter == NUM_THREADS * TASKS_PER_THREAD * 2, 
                "Concurrent execution failed");

    ppdb_async_loop_destroy(loop);
    return 0;
}

int main(void) {
    int result = 0;

    printf("Running async loop tests...\n");
    result = test_async_loop();
    if (result != 0) {
        return result;
    }
    printf("Async loop tests passed.\n");

    printf("Running async task tests...\n");
    result = test_async_task();
    if (result != 0) {
        return result;
    }
    printf("Async task tests passed.\n");

    printf("Running async cancel tests...\n");
    result = test_async_cancel();
    if (result != 0) {
        return result;
    }
    printf("Async cancel tests passed.\n");

    printf("Running async IO tests...\n");
    result = test_async_io();
    if (result != 0) {
        return result;
    }
    printf("Async IO tests passed.\n");

    printf("Running performance tests...\n");
    result = test_async_performance();
    if (result != 0) return result;
    printf("Performance tests passed.\n");

    printf("Running boundary condition tests...\n");
    result = test_async_boundary_conditions();
    if (result != 0) return result;
    printf("Boundary condition tests passed.\n");

    printf("Running concurrent tests...\n");
    result = test_async_concurrent();
    if (result != 0) return result;
    printf("Concurrent tests passed.\n");

    return 0;
} 