#include "test_common.h"
#include "internal/infra/infra.h"
#include "test_framework.h"

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
    // 测试基本分配和释放
    void* ptr = infra_malloc(100);
    TEST_ASSERT(ptr != NULL);
    infra_memset(ptr, 0xAA, 100);
    infra_free(ptr);

    // 测试零大小分配（应该返回一个有效的指针）
    ptr = infra_malloc(0);
    TEST_ASSERT(ptr != NULL);
    infra_free(ptr);

    // 测试大内存分配
    ptr = infra_malloc(1024 * 1024);
    TEST_ASSERT(ptr != NULL);
    infra_free(ptr);

    return 0;
}

// 内存操作测试
static int test_memory_operations(void) {
    // 测试memset
    void* ptr = infra_malloc(100);
    TEST_ASSERT(ptr != NULL);
    infra_memset(ptr, 0xAA, 100);
    
    // 测试memcpy
    void* dest = infra_malloc(100);
    TEST_ASSERT(dest != NULL);
    infra_memcpy(dest, ptr, 100);
    TEST_ASSERT(infra_memcmp(ptr, dest, 100) == 0);
    
    // 测试memmove（重叠区域）
    infra_memmove(ptr + 10, ptr, 50);
    
    infra_free(ptr);
    infra_free(dest);
    return 0;
}

// 内存性能测试
static int test_memory_performance(void) {
    const int iterations = 1000;
    const int sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024};
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    infra_time_t start = infra_time_monotonic();
    
    for (int i = 0; i < iterations; i++) {
        for (int j = 0; j < num_sizes; j++) {
            void* ptr = infra_malloc(sizes[j]);
            TEST_ASSERT(ptr != NULL);
            infra_memset(ptr, 0xAA, sizes[j]);
            infra_free(ptr);
            
            g_stats.total_allocs++;
            g_stats.total_frees++;
            g_stats.total_bytes += sizes[j];
            g_stats.avg_alloc_size = (double)g_stats.total_bytes / g_stats.total_allocs;
        }
    }
    
    infra_time_t end = infra_time_monotonic();
    double time_spent = (double)(end - start) / 1000000.0;  // Convert to seconds
    TEST_ASSERT(time_spent < 30.0);  // 性能测试应在30秒内完成
    
    return 0;
}

// 内存压力测试
static int test_memory_stress(void) {
    const int iterations = 100;
    const int max_allocs = 1000;
    void* ptrs[1000];
    
    for (int i = 0; i < iterations; i++) {
        // 随机分配
        int num_allocs = rand() % max_allocs + 1;
        for (int j = 0; j < num_allocs; j++) {
            size_t size = rand() % 4096 + 1;
            ptrs[j] = infra_malloc(size);
            TEST_ASSERT(ptrs[j] != NULL);
            infra_memset(ptrs[j], 0xAA, size);
            g_stats.current_bytes += size;
            if (g_stats.current_bytes > g_stats.peak_bytes) {
                g_stats.peak_bytes = g_stats.current_bytes;
            }
        }
        
        // 随机释放
        for (int j = 0; j < num_allocs; j++) {
            infra_free(ptrs[j]);
        }
    }
    
    return 0;
}

int main(void) {
    // 初始化infra系统
    infra_error_t err = infra_init();
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize infra system: %d\n", err);
        return 1;
    }

    TEST_INIT();
    
    TEST_RUN(test_memory_basic);
    TEST_RUN(test_memory_operations);
    TEST_RUN(test_memory_performance);
    TEST_RUN(test_memory_stress);
    
    TEST_CLEANUP();
    
    // 打印内存统计信息
    infra_printf("\nMemory Statistics:\n");
    infra_printf("Total allocations: %ld\n", g_stats.total_allocs);
    infra_printf("Total frees: %ld\n", g_stats.total_frees);
    infra_printf("Total bytes allocated: %ld\n", g_stats.total_bytes);
    infra_printf("Peak memory usage: %ld bytes\n", g_stats.peak_bytes);
    infra_printf("Average allocation size: %.2f bytes\n", g_stats.avg_alloc_size);
    
    // 清理infra系统
    infra_cleanup();
    return 0;
} 