#include <cosmopolitan.h>
#include "internal/base.h"

#define TEST_ITERATIONS 10000
#define TEST_BUFFER_SIZE 4096
#define WARMUP_ITERATIONS 1000

static char test_buffer[TEST_BUFFER_SIZE];
static int completion_count = 0;

static void perf_callback(ppdb_base_async_t* async, void* data, size_t bytes, ppdb_error_t error) {
    completion_count++;
}

// 测试异步IO操作延迟
static void test_async_latency(void) {
    ppdb_error_t err;
    ppdb_base_async_t* async = NULL;
    int64_t start_time, end_time;
    double avg_latency;
    const char* test_file = "test_async_perf.dat";
    int fd;

    // 创建测试文件
    fd = open(test_file, O_CREAT | O_RDWR, 0644);
    assert(fd != -1);
    
    // 初始化异步IO管理器
    err = ppdb_base_async_create(&async);
    assert(err == PPDB_OK);

    // 预热
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        err = ppdb_base_async_write(async, fd, test_buffer, TEST_BUFFER_SIZE, 0, perf_callback, NULL);
        assert(err == PPDB_OK);
        err = ppdb_base_async_wait(async, 1000);
        assert(err == PPDB_OK);
    }

    // 延迟测试
    start_time = now_usec();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        err = ppdb_base_async_write(async, fd, test_buffer, TEST_BUFFER_SIZE, 0, perf_callback, NULL);
        assert(err == PPDB_OK);
        err = ppdb_base_async_wait(async, 1000);
        assert(err == PPDB_OK);
    }
    end_time = now_usec();

    avg_latency = (double)(end_time - start_time) / TEST_ITERATIONS;
    printf("Average async write latency: %.2f us\n", avg_latency);

    // 清理
    close(fd);
    unlink(test_file);
    ppdb_base_async_destroy(async);
}

// 测试异步IO操作吞吐量
static void test_async_throughput(void) {
    ppdb_error_t err;
    ppdb_base_async_t* async = NULL;
    int64_t start_time, end_time;
    double throughput;
    const char* test_file = "test_async_perf.dat";
    int fd;

    // 创建测试文件
    fd = open(test_file, O_CREAT | O_RDWR, 0644);
    assert(fd != -1);
    
    // 初始化异步IO管理器
    err = ppdb_base_async_create(&async);
    assert(err == PPDB_OK);

    // 预热
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        err = ppdb_base_async_write(async, fd, test_buffer, TEST_BUFFER_SIZE, 0, perf_callback, NULL);
        assert(err == PPDB_OK);
    }
    err = ppdb_base_async_wait_all(async);
    assert(err == PPDB_OK);

    // 吞吐量测试
    completion_count = 0;
    start_time = now_usec();
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        err = ppdb_base_async_write(async, fd, test_buffer, TEST_BUFFER_SIZE, 0, perf_callback, NULL);
        assert(err == PPDB_OK);
    }
    err = ppdb_base_async_wait_all(async);
    assert(err == PPDB_OK);
    end_time = now_usec();

    throughput = ((double)TEST_ITERATIONS * TEST_BUFFER_SIZE) / (end_time - start_time);
    printf("Async write throughput: %.2f MB/s\n", throughput);

    // 清理
    close(fd);
    unlink(test_file);
    ppdb_base_async_destroy(async);
}

int main(void) {
    printf("\n=== Running async performance tests ===\n");
    
    printf("\nTesting async IO latency...\n");
    test_async_latency();
    
    printf("\nTesting async IO throughput...\n");
    test_async_throughput();
    
    return 0;
} 