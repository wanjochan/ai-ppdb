#include "test/test_common.h"
#include "internal/infra/infra_memory.h"
#include "test/test_framework.h"

// 性能统计结构
typedef struct {
    int64_t total_allocs;
    int64_t total_frees;
    int64_t total_bytes;
    int64_t peak_bytes;
    int64_t current_bytes;
    double avg_alloc_size;
} mem_stats_t;

static mem_stats_t g_stats = {0};

// 基本内存分配测试
static int test_memory_basic(void) {
    infra_printf("Running basic memory tests...\n");

    // 初始化内存管理
    infra_config_t config;
    infra_error_t err = infra_config_init(&config);
    TEST_ASSERT(err == INFRA_OK, "Config initialization failed");

    config.memory.use_memory_pool = false;
    config.memory.pool_initial_size = 0;
    config.memory.pool_alignment = sizeof(void*);

    err = infra_init_with_config((infra_init_flags_t)INFRA_INIT_MEMORY, &config);
    TEST_ASSERT(err == INFRA_OK, "Memory initialization failed");

    // 测试基本分配和释放
    void* ptr = infra_malloc(100);
    TEST_ASSERT(ptr != NULL, "Memory allocation failed");
    infra_memset(ptr, 0xAA, 100);
    infra_free(ptr);

    // 测试零大小分配
    ptr = infra_malloc(0);
    TEST_ASSERT(ptr == NULL, "Zero size allocation should return NULL");

    // 测试大内存分配
    ptr = infra_malloc(1024 * 1024);
    TEST_ASSERT(ptr != NULL, "Large memory allocation failed");
    infra_free(ptr);

    // 清理内存管理
    infra_cleanup();

    infra_printf("Basic memory tests passed\n");
    return 0;
}

// 内存对齐测试
static int test_memory_alignment(void) {
    infra_printf("Running memory alignment tests...\n");

    // 初始化内存管理
    infra_config_t config;
    infra_error_t err = infra_config_init(&config);
    TEST_ASSERT(err == INFRA_OK, "Config initialization failed");

    config.memory.use_memory_pool = false;
    config.memory.pool_initial_size = 0;
    config.memory.pool_alignment = 32;  // 使用最大对齐要求

    err = infra_init_with_config((infra_init_flags_t)INFRA_INIT_MEMORY, &config);
    TEST_ASSERT(err == INFRA_OK, "Memory initialization failed");

    // 测试不同大小的分配是否都正确对齐
    void* ptr1 = infra_malloc(100);
    TEST_ASSERT(((uintptr_t)ptr1 % 32) == 0, "32-byte alignment failed");
    infra_free(ptr1);

    void* ptr2 = infra_malloc(200);
    TEST_ASSERT(((uintptr_t)ptr2 % 32) == 0, "32-byte alignment failed");
    infra_free(ptr2);

    void* ptr3 = infra_malloc(300);
    TEST_ASSERT(((uintptr_t)ptr3 % 32) == 0, "32-byte alignment failed");
    infra_free(ptr3);

    // 清理内存管理
    infra_cleanup();

    infra_printf("Memory alignment tests passed\n");
    return 0;
}

// 内存池测试
static int test_memory_pool(void) {
    infra_printf("Running memory pool tests...\n");

    // 初始化内存管理
    infra_config_t config;
    infra_error_t err = infra_config_init(&config);
    TEST_ASSERT(err == INFRA_OK, "Config initialization failed");

    config.memory.use_memory_pool = true;
    config.memory.pool_initial_size = 1024 * 1024;  // 1MB
    config.memory.pool_alignment = sizeof(void*);

    err = infra_init_with_config((infra_init_flags_t)INFRA_INIT_MEMORY, &config);
    TEST_ASSERT(err == INFRA_OK, "Memory initialization failed");

    // 分配多个块
    void* blocks[64];
    for (int i = 0; i < 64; i++) {
        blocks[i] = infra_malloc(16);
        TEST_ASSERT(blocks[i] != NULL, "Pool allocation failed");
        infra_memset(blocks[i], i, 16);
    }

    // 释放一半的块
    for (int i = 0; i < 32; i++) {
        infra_free(blocks[i]);
    }

    // 重新分配
    for (int i = 0; i < 32; i++) {
        blocks[i] = infra_malloc(16);
        TEST_ASSERT(blocks[i] != NULL, "Pool reallocation failed");
    }

    // 释放所有块
    for (int i = 0; i < 64; i++) {
        infra_free(blocks[i]);
    }

    // 清理内存管理
    infra_cleanup();

    infra_printf("Memory pool tests passed\n");
    return 0;
}

// 内存性能测试
static int test_memory_performance(void) {
    infra_printf("Running memory performance tests...\n");

    // 初始化内存管理
    infra_config_t config;
    infra_error_t err = infra_config_init(&config);
    TEST_ASSERT(err == INFRA_OK, "Config initialization failed");

    config.memory.use_memory_pool = false;
    config.memory.pool_initial_size = 0;
    config.memory.pool_alignment = sizeof(void*);

    err = infra_init_with_config((infra_init_flags_t)INFRA_INIT_MEMORY, &config);
    TEST_ASSERT(err == INFRA_OK, "Memory initialization failed");

    const int NUM_ALLOCS = 10000;
    const int MAX_SIZE = 1024;
    void* ptrs[NUM_ALLOCS];
    int64_t start_time, end_time;
    double total_time;

    // 测试连续分配
    start_time = infra_get_time_ms();
    for (int i = 0; i < NUM_ALLOCS; i++) {
        size_t size = infra_random() % MAX_SIZE + 1;
        ptrs[i] = infra_malloc(size);
        TEST_ASSERT(ptrs[i] != NULL, "Performance allocation failed");
        g_stats.total_allocs++;
        g_stats.total_bytes += size;
        g_stats.current_bytes += size;
        g_stats.peak_bytes = MAX(g_stats.peak_bytes, g_stats.current_bytes);
    }
    end_time = infra_get_time_ms();
    total_time = (end_time - start_time) / 1000.0;
    infra_printf("Allocation rate: %.2f allocs/sec\n", NUM_ALLOCS / total_time);

    // 测试连续释放
    start_time = infra_get_time_ms();
    for (int i = 0; i < NUM_ALLOCS; i++) {
        infra_free(ptrs[i]);
        g_stats.total_frees++;
        g_stats.current_bytes -= MAX_SIZE;
    }
    end_time = infra_get_time_ms();
    total_time = (end_time - start_time) / 1000.0;
    infra_printf("Free rate: %.2f frees/sec\n", NUM_ALLOCS / total_time);

    // 清理内存管理
    infra_cleanup();

    infra_printf("Memory performance tests passed\n");
    return 0;
}

// 打印内存统计信息
static void print_memory_stats(void) {
    infra_memory_stats_t stats;
    if (infra_memory_get_stats(&stats) == INFRA_OK) {
        infra_printf("\n=== Memory Statistics ===\n");
        infra_printf("Current usage: %zu bytes\n", stats.current_usage);
        infra_printf("Peak usage: %zu bytes\n", stats.peak_usage);
        infra_printf("Total allocations: %zu\n", stats.total_allocations);
        infra_printf("Pool utilization: %zu%%\n", stats.pool_utilization);
        infra_printf("Pool fragmentation: %zu%%\n", stats.pool_fragmentation);
        infra_printf("=====================\n\n");
    }
}

// 主测试函数
int main(void) {
    infra_printf("Running memory tests...\n");

    // 运行所有测试
    TEST_RUN(test_memory_basic);
    TEST_RUN(test_memory_alignment);
    TEST_RUN(test_memory_pool);
    TEST_RUN(test_memory_performance);

    // 打印最终统计信息
    print_memory_stats();

    infra_printf("All memory tests passed!\n");
    return 0;
} 