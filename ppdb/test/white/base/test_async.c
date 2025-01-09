#include <cosmopolitan.h>
#include "internal/base.h"

// Test data
static int io_complete_count = 0;
static char test_buffer[1024];
static const char test_data[] = "Hello, Async IO!";

// IO completion callback
static void test_io_callback(ppdb_base_async_t* async, void* data, size_t bytes_transferred, ppdb_error_t error) {
    io_complete_count++;
}

// Test async IO basic operations
static void test_async_basic(void) {
    ppdb_error_t err;
    ppdb_base_async_t* async = NULL;
    ppdb_base_async_stats_t stats;

    // Create async IO manager
    err = ppdb_base_async_create(&async);
    assert(err == PPDB_OK);
    assert(async != NULL);

    // Check initial stats
    ppdb_base_async_get_stats(async, &stats);
    assert(stats.total_operations == 0);
    assert(stats.active_operations == 0);
    assert(stats.total_bytes_read == 0);
    assert(stats.total_bytes_written == 0);

    // Cleanup
    ppdb_base_async_destroy(async);
}

// Test async read operations
static void test_async_read(void) {
    ppdb_error_t err;
    ppdb_base_async_t* async = NULL;
    ppdb_base_async_stats_t stats;
    int fd;
    const char* test_file = "test_async_read.txt";

    // Create async IO manager
    err = ppdb_base_async_create(&async);
    assert(err == PPDB_OK);

    // Create test file
    fd = open(test_file, O_CREAT | O_WRONLY, 0644);
    assert(fd != -1);
    assert(write(fd, test_data, strlen(test_data)) == strlen(test_data));
    close(fd);

    // Open file for reading
    fd = open(test_file, O_RDONLY);
    assert(fd != -1);

    // Start async read
    io_complete_count = 0;
    memset(test_buffer, 0, sizeof(test_buffer));
    err = ppdb_base_async_read(async, fd, test_buffer, strlen(test_data), 0, test_io_callback, NULL);
    assert(err == PPDB_OK);

    // Wait for completion
    err = ppdb_base_async_wait(async, 1000);
    assert(err == PPDB_OK);

    // Check results
    assert(io_complete_count == 1);
    assert(memcmp(test_buffer, test_data, strlen(test_data)) == 0);

    // Check stats
    ppdb_base_async_get_stats(async, &stats);
    assert(stats.total_operations == 1);
    assert(stats.active_operations == 0);
    assert(stats.total_bytes_read == strlen(test_data));

    // Cleanup
    close(fd);
    unlink(test_file);
    ppdb_base_async_destroy(async);
}

// Test async write operations
static void test_async_write(void) {
    ppdb_error_t err;
    ppdb_base_async_t* async = NULL;
    ppdb_base_async_stats_t stats;
    int fd;
    const char* test_file = "test_async_write.txt";
    char read_buffer[1024];

    // Create async IO manager
    err = ppdb_base_async_create(&async);
    assert(err == PPDB_OK);

    // Create file for writing
    fd = open(test_file, O_CREAT | O_RDWR, 0644);
    assert(fd != -1);

    // Start async write
    io_complete_count = 0;
    err = ppdb_base_async_write(async, fd, test_data, strlen(test_data), 0, test_io_callback, NULL);
    assert(err == PPDB_OK);

    // Wait for completion
    err = ppdb_base_async_wait(async, 1000);
    assert(err == PPDB_OK);

    // Check results
    assert(io_complete_count == 1);
    
    // Verify written data
    lseek(fd, 0, SEEK_SET);
    memset(read_buffer, 0, sizeof(read_buffer));
    assert(read(fd, read_buffer, strlen(test_data)) == strlen(test_data));
    assert(memcmp(read_buffer, test_data, strlen(test_data)) == 0);

    // Check stats
    ppdb_base_async_get_stats(async, &stats);
    assert(stats.total_operations == 1);
    assert(stats.active_operations == 0);
    assert(stats.total_bytes_written == strlen(test_data));

    // Cleanup
    close(fd);
    unlink(test_file);
    ppdb_base_async_destroy(async);
}

// Test error handling
static void test_async_errors(void) {
    ppdb_error_t err;
    ppdb_base_async_t* async = NULL;

    // Test invalid parameters
    err = ppdb_base_async_create(NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_async_create(&async);
    assert(err == PPDB_OK);

    // Test invalid file descriptor
    err = ppdb_base_async_read(async, -1, test_buffer, sizeof(test_buffer), 0, test_io_callback, NULL);
    assert(err == PPDB_BASE_ERR_IO);

    // Test NULL buffer
    err = ppdb_base_async_read(async, 0, NULL, sizeof(test_buffer), 0, test_io_callback, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Test zero length
    err = ppdb_base_async_read(async, 0, test_buffer, 0, 0, test_io_callback, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Test NULL callback
    err = ppdb_base_async_read(async, 0, test_buffer, sizeof(test_buffer), 0, NULL, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Cleanup
    ppdb_base_async_destroy(async);
}

// 优先级任务测试
static void test_priority_callback(ppdb_error_t error, void* arg) {
    int* priority = (int*)arg;
    printf("Task with priority %d completed with error %d\n", *priority, error);
}

static int test_async_priority(void) {
    printf("\n=== Running async priority tests ===\n");
    
    ppdb_base_async_loop_t* loop = NULL;
    ASSERT_OK(ppdb_base_async_loop_create(&loop, 4));
    
    // 提交不同优先级的任务
    ppdb_base_async_handle_t* handles[3];
    int priorities[3] = {0, 1, 2};  // HIGH, NORMAL, LOW
    
    ASSERT_OK(ppdb_base_async_submit(loop, test_task, &priorities[0], 
        PPDB_ASYNC_PRIORITY_HIGH, 1000000, test_priority_callback, &priorities[0], &handles[0]));
    
    ASSERT_OK(ppdb_base_async_submit(loop, test_task, &priorities[1],
        PPDB_ASYNC_PRIORITY_NORMAL, 1000000, test_priority_callback, &priorities[1], &handles[1]));
    
    ASSERT_OK(ppdb_base_async_submit(loop, test_task, &priorities[2],
        PPDB_ASYNC_PRIORITY_LOW, 1000000, test_priority_callback, &priorities[2], &handles[2]));
    
    // 等待所有任务完成
    ppdb_base_async_wait_all(loop);
    
    // 测试取消任务
    ASSERT_OK(ppdb_base_async_submit(loop, test_task, NULL,
        PPDB_ASYNC_PRIORITY_NORMAL, 1000000, NULL, NULL, &handles[0]));
    ASSERT_OK(ppdb_base_async_cancel(handles[0]));
    
    ppdb_base_async_loop_destroy(loop);
    return 0;
}

int main(void) {
    printf("Testing async IO basic operations...\n");
    test_async_basic();
    printf("PASSED\n");

    printf("Testing async read operations...\n");
    test_async_read();
    printf("PASSED\n");

    printf("Testing async write operations...\n");
    test_async_write();
    printf("PASSED\n");

    printf("Testing async error handling...\n");
    test_async_errors();
    printf("PASSED\n");
    
    printf("Testing async priority handling...\n");
    test_async_priority();
    printf("PASSED\n");

    return 0;
}