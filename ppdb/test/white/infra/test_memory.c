#include "test/test_common.h"
#include "internal/infra/infra.h"
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
    printf("Running basic memory tests...\n");

    // 测试基本分配和释放
    void* ptr = ppdb_mem_malloc(100);
    TEST_ASSERT(ptr != NULL, "Memory allocation failed");
    memset(ptr, 0xAA, 100);
    ppdb_mem_free(ptr);

    // 测试零大小分配
    ptr = ppdb_mem_malloc(0);
    TEST_ASSERT(ptr == NULL, "Zero size allocation should return NULL");

    // 测试大内存分配
    ptr = ppdb_mem_malloc(1024 * 1024);
    TEST_ASSERT(ptr != NULL, "Large memory allocation failed");
    ppdb_mem_free(ptr);

    printf("Basic memory tests passed\n");
    return 0;
}

// 内存对齐测试
static int test_memory_alignment(void) {
    printf("Running memory alignment tests...\n");

    // 测试不同对齐要求
    void* ptr1 = ppdb_mem_aligned_alloc(8, 100);
    TEST_ASSERT(((uintptr_t)ptr1 % 8) == 0, "8-byte alignment failed");
    ppdb_mem_free(ptr1);

    void* ptr2 = ppdb_mem_aligned_alloc(16, 100);
    TEST_ASSERT(((uintptr_t)ptr2 % 16) == 0, "16-byte alignment failed");
    ppdb_mem_free(ptr2);

    void* ptr3 = ppdb_mem_aligned_alloc(32, 100);
    TEST_ASSERT(((uintptr_t)ptr3 % 32) == 0, "32-byte alignment failed");
    ppdb_mem_free(ptr3);

    printf("Memory alignment tests passed\n");
    return 0;
}

// 内存池测试
static int test_memory_pool(void) {
    printf("Running memory pool tests...\n");

    ppdb_mempool_t* pool;
    ppdb_error_t err;

    // 创建内存池
    err = ppdb_mempool_create(&pool, 1024, 16);
    TEST_ASSERT(err == PPDB_OK, "Memory pool creation failed");

    // 分配多个块
    void* blocks[64];
    for (int i = 0; i < 64; i++) {
        blocks[i] = ppdb_mempool_alloc(pool, 16);
        TEST_ASSERT(blocks[i] != NULL, "Pool allocation failed");
        memset(blocks[i], i, 16);
    }

    // 释放一半的块
    for (int i = 0; i < 32; i++) {
        ppdb_mempool_free(pool, blocks[i]);
    }

    // 重新分配
    for (int i = 0; i < 32; i++) {
        blocks[i] = ppdb_mempool_alloc(pool, 16);
        TEST_ASSERT(blocks[i] != NULL, "Pool reallocation failed");
    }

    // 释放所有块
    for (int i = 0; i < 64; i++) {
        ppdb_mempool_free(pool, blocks[i]);
    }

    // 销毁内存池
    ppdb_mempool_destroy(pool);

    printf("Memory pool tests passed\n");
    return 0;
}

// 内存性能测试
static int test_memory_performance(void) {
    printf("Running memory performance tests...\n");

    const int NUM_ALLOCS = 10000;
    const int MAX_SIZE = 1024;
    void* ptrs[NUM_ALLOCS];
    int64_t start_time, end_time;
    double total_time;

    // 测试连续分配
    start_time = now_usec();
    for (int i = 0; i < NUM_ALLOCS; i++) {
        size_t size = rand() % MAX_SIZE + 1;
        ptrs[i] = ppdb_mem_malloc(size);
        TEST_ASSERT(ptrs[i] != NULL, "Performance allocation failed");
        g_stats.total_allocs++;
        g_stats.total_bytes += size;
        g_stats.current_bytes += size;
        g_stats.peak_bytes = MAX(g_stats.peak_bytes, g_stats.current_bytes);
    }
    end_time = now_usec();
    total_time = (end_time - start_time) / 1000000.0;
    printf("Allocation rate: %.2f allocs/sec\n", NUM_ALLOCS / total_time);

    // 测试连续释放
    start_time = now_usec();
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ppdb_mem_free(ptrs[i]);
        g_stats.total_frees++;
        g_stats.current_bytes -= MAX_SIZE;
    }
    end_time = now_usec();
    total_time = (end_time - start_time) / 1000000.0;
    printf("Free rate: %.2f frees/sec\n", NUM_ALLOCS / total_time);

    printf("Memory performance tests passed\n");
    return 0;
}

// 内存压力测试
static void* stress_thread_func(void* arg) {
    const int NUM_ITERS = 1000;
    const int MAX_ALLOCS = 100;
    void* ptrs[MAX_ALLOCS];
    int num_allocs = 0;

    for (int i = 0; i < NUM_ITERS; i++) {
        // 随机分配或释放
        if (rand() % 2 == 0 && num_allocs < MAX_ALLOCS) {
            size_t size = rand() % 1024 + 1;
            ptrs[num_allocs] = ppdb_mem_malloc(size);
            if (ptrs[num_allocs] != NULL) {
                num_allocs++;
            }
        } else if (num_allocs > 0) {
            num_allocs--;
            ppdb_mem_free(ptrs[num_allocs]);
        }
    }

    // 清理剩余内存
    for (int i = 0; i < num_allocs; i++) {
        ppdb_mem_free(ptrs[i]);
    }

    return NULL;
}

static int test_memory_stress(void) {
    printf("Running memory stress tests...\n");

    const int NUM_THREADS = 4;
    ppdb_thread_t* threads[NUM_THREADS];

    // 创建多个线程进行压力测试
    for (int i = 0; i < NUM_THREADS; i++) {
        ppdb_error_t err = ppdb_thread_create(&threads[i], stress_thread_func, NULL);
        TEST_ASSERT(err == PPDB_OK, "Thread creation failed");
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        ppdb_thread_join(threads[i]);
    }

    printf("Memory stress tests passed\n");
    return 0;
}

// 打印内存统计信息
static void print_memory_stats(void) {
    printf("\n=== Memory Statistics ===\n");
    printf("Total allocations: %ld\n", g_stats.total_allocs);
    printf("Total frees: %ld\n", g_stats.total_frees);
    printf("Total bytes allocated: %ld\n", g_stats.total_bytes);
    printf("Peak memory usage: %ld bytes\n", g_stats.peak_bytes);
    printf("Average allocation size: %.2f bytes\n", 
           g_stats.total_allocs > 0 ? (double)g_stats.total_bytes / g_stats.total_allocs : 0);
    printf("=====================\n\n");
}

int main(void) {
    int result = 0;

    // 运行所有测试
    result |= test_memory_basic();
    result |= test_memory_alignment();
    result |= test_memory_pool();
    result |= test_memory_performance();
    result |= test_memory_stress();

    // 打印统计信息
    print_memory_stats();

    return result;
} 