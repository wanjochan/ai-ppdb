#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_async.h"

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        printf("ASSERT FAILED: %s\n", msg); \
        return 1; \
    }

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
    char buf[128];
    ppdb_error_t err;
    int fd;

    // Create loop
    err = ppdb_async_loop_create(&loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop creation failed");

    // Create test file
    fd = open("test.txt", O_CREAT | O_RDWR, 0644);
    TEST_ASSERT(fd >= 0, "File creation failed");

    // Write test
    const char* test_data = "Hello, World!";
    err = ppdb_async_write(loop, fd, test_data, strlen(test_data), 0,
                          test_callback, &counter);
    TEST_ASSERT(err == PPDB_OK, "Async write failed");

    // Run loop
    err = ppdb_async_loop_run(loop, 1000);
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");
    TEST_ASSERT(counter == 1, "Write callback not called");

    // Read test
    err = ppdb_async_read(loop, fd, buf, strlen(test_data), 0,
                         test_callback, &counter);
    TEST_ASSERT(err == PPDB_OK, "Async read failed");

    // Run loop
    err = ppdb_async_loop_run(loop, 1000);
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");
    TEST_ASSERT(counter == 2, "Read callback not called");

    // Compare data
    TEST_ASSERT(memcmp(buf, test_data, strlen(test_data)) == 0,
                "Read data does not match written data");

    // Fsync test
    err = ppdb_async_fsync(loop, fd, test_callback, &counter);
    TEST_ASSERT(err == PPDB_OK, "Async fsync failed");

    // Run loop
    err = ppdb_async_loop_run(loop, 1000);
    TEST_ASSERT(err == PPDB_OK, "Loop run failed");
    TEST_ASSERT(counter == 3, "Fsync callback not called");

    // Cleanup
    close(fd);
    unlink("test.txt");
    err = ppdb_async_loop_destroy(loop);
    TEST_ASSERT(err == PPDB_OK, "Async loop destroy failed");

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

    return 0;
} 