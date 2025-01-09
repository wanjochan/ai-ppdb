#include <cosmopolitan.h>
#include "internal/base.h"
#include "../test_framework.h"

// 全局测试数据
static ppdb_base_t* g_base = NULL;

// 内存统计数据
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;
    int64_t alloc_count;
    int64_t free_count;
} mem_stats_t;

// 测试初始化
static int test_setup(void) {
    printf("\n=== Setting up memory test environment ===\n");
    
    // 初始化 base 配置
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .thread_pool_size = 4,
        .thread_safe = true,
        .enable_logging = true,
        .log_level = PPDB_LOG_DEBUG
    };
    
    // 初始化 base 层
    ASSERT_OK(ppdb_base_init(&g_base, &base_config));
    
    printf("Test environment setup completed\n");
    return 0;
}

// 测试清理
static int test_teardown(void) {
    printf("\n=== Cleaning up memory test environment ===\n");
    
    if (g_base) {
        ppdb_base_destroy(g_base);
        g_base = NULL;
    }
    
    printf("Test environment cleanup completed\n");
    return 0;
}

// 初始化内存统计
static void init_mem_stats(mem_stats_t* stats) {
    stats->total_allocated = 0;
    stats->total_freed = 0;
    stats->current_usage = 0;
    stats->peak_usage = 0;
    stats->alloc_count = 0;
    stats->free_count = 0;
}

// 更新内存统计
static void update_mem_stats_alloc(mem_stats_t* stats, size_t size) {
    stats->total_allocated += size;
    stats->current_usage += size;
    stats->peak_usage = MAX(stats->peak_usage, stats->current_usage);
    stats->alloc_count++;
}

static void update_mem_stats_free(mem_stats_t* stats, size_t size) {
    stats->total_freed += size;
    stats->current_usage -= size;
    stats->free_count++;
}

// 打印内存统计
static void print_mem_stats(const char* test_name, mem_stats_t* stats) {
    printf("\n=== Memory Statistics for %s ===\n", test_name);
    printf("Total Allocated: %zu bytes\n", stats->total_allocated);
    printf("Total Freed: %zu bytes\n", stats->total_freed);
    printf("Current Usage: %zu bytes\n", stats->current_usage);
    printf("Peak Usage: %zu bytes\n", stats->peak_usage);
    printf("Allocation Count: %ld\n", stats->alloc_count);
    printf("Free Count: %ld\n", stats->free_count);
    printf("=====================================\n");
}

// 基本内存分配测试
static int test_memory_basic(void) {
    printf("\n=== Running basic memory tests ===\n");
    
    mem_stats_t stats;
    init_mem_stats(&stats);
    
    // 测试小块内存
    void* small_block = ppdb_base_malloc(128);
    ASSERT_NOT_NULL(small_block);
    update_mem_stats_alloc(&stats, 128);
    
    // 测试中等块内存
    void* medium_block = ppdb_base_malloc(4096);
    ASSERT_NOT_NULL(medium_block);
    update_mem_stats_alloc(&stats, 4096);
    
    // 测试大块内存
    void* large_block = ppdb_base_malloc(1024 * 1024);
    ASSERT_NOT_NULL(large_block);
    update_mem_stats_alloc(&stats, 1024 * 1024);
    
    // 释放内存
    ppdb_base_free(small_block);
    update_mem_stats_free(&stats, 128);
    
    ppdb_base_free(medium_block);
    update_mem_stats_free(&stats, 4096);
    
    ppdb_base_free(large_block);
    update_mem_stats_free(&stats, 1024 * 1024);
    
    print_mem_stats("Basic Memory Test", &stats);
    return 0;
}

// 内存重分配测试
static int test_memory_realloc(void) {
    printf("\n=== Running memory reallocation tests ===\n");
    
    mem_stats_t stats;
    init_mem_stats(&stats);
    
    // 初始分配
    void* block = ppdb_base_malloc(256);
    ASSERT_NOT_NULL(block);
    update_mem_stats_alloc(&stats, 256);
    
    // 扩大内存
    block = ppdb_base_realloc(block, 512);
    ASSERT_NOT_NULL(block);
    update_mem_stats_alloc(&stats, 512);
    update_mem_stats_free(&stats, 256);
    
    // 缩小内存
    block = ppdb_base_realloc(block, 128);
    ASSERT_NOT_NULL(block);
    update_mem_stats_alloc(&stats, 128);
    update_mem_stats_free(&stats, 512);
    
    // 释放内存
    ppdb_base_free(block);
    update_mem_stats_free(&stats, 128);
    
    print_mem_stats("Memory Reallocation Test", &stats);
    return 0;
}

// 内存对齐测试
static int test_memory_alignment(void) {
    printf("\n=== Running memory alignment tests ===\n");
    
    mem_stats_t stats;
    init_mem_stats(&stats);
    
    // 测试不同的对齐大小
    for (size_t align = 8; align <= 4096; align *= 2) {
        void* block = NULL;
        size_t size = align * 2;
        
        ASSERT_OK(ppdb_base_memalign(&block, align, size));
        ASSERT_NOT_NULL(block);
        update_mem_stats_alloc(&stats, size);
        
        // 验证对齐
        uintptr_t addr = (uintptr_t)block;
        ASSERT_EQ(addr % align, 0);
        
        ppdb_base_free(block);
        update_mem_stats_free(&stats, size);
    }
    
    print_mem_stats("Memory Alignment Test", &stats);
    return 0;
}

// 内存边界测试
static int test_memory_boundary(void) {
    printf("\n=== Running memory boundary tests ===\n");
    
    mem_stats_t stats;
    init_mem_stats(&stats);
    
    // 测试零大小分配
    void* zero_block = ppdb_base_malloc(0);
    ASSERT_NOT_NULL(zero_block);
    update_mem_stats_alloc(&stats, 0);
    
    ppdb_base_free(zero_block);
    update_mem_stats_free(&stats, 0);
    
    // 测试最大可分配大小
    size_t max_size = 1024 * 1024 * 8;  // 8MB
    void* large_block = ppdb_base_malloc(max_size);
    ASSERT_NOT_NULL(large_block);
    update_mem_stats_alloc(&stats, max_size);
    
    ppdb_base_free(large_block);
    update_mem_stats_free(&stats, max_size);
    
    print_mem_stats("Memory Boundary Test", &stats);
    return 0;
}

// 内存池测试
static int test_memory_pool(void) {
    printf("\n=== Running memory pool tests ===\n");
    
    mem_stats_t stats;
    init_mem_stats(&stats);
    
    // 创建内存池
    ppdb_base_mempool_t* pool = NULL;
    ASSERT_OK(ppdb_base_mempool_create(&pool, 4096, 8));
    
    // 测试内存池分配
    void* blocks[10];
    for(int i = 0; i < 10; i++) {
        blocks[i] = ppdb_base_mempool_alloc(pool, 256);
        ASSERT_NOT_NULL(blocks[i]);
        update_mem_stats_alloc(&stats, 256);
    }
    
    // 获取内存池统计
    ppdb_base_mempool_stats_t pool_stats;
    ppdb_base_mempool_get_stats(pool, &pool_stats);
    
    // 验证统计数据
    ASSERT_TRUE(pool_stats.total_size >= 4096);
    ASSERT_TRUE(pool_stats.used_size >= 2560);
    ASSERT_TRUE(pool_stats.block_count > 0);
    
    // 销毁内存池
    ASSERT_OK(ppdb_base_mempool_destroy(pool));
    
    print_mem_stats("Memory Pool Test", &stats);
    return 0;
}

int main(void) {
    if (test_setup() != 0) {
        printf("Test setup failed\n");
        return 1;
    }
    
    TEST_CASE(test_memory_basic);
    TEST_CASE(test_memory_realloc);
    TEST_CASE(test_memory_alignment);
    TEST_CASE(test_memory_boundary);
    TEST_CASE(test_memory_pool);
    
    if (test_teardown() != 0) {
        printf("Test teardown failed\n");
        return 1;
    }
    
    printf("\nTest summary:\n");
    printf("  Total: %d\n", g_test_count);
    printf("  Passed: %d\n", g_test_passed);
    printf("  Failed: %d\n", g_test_failed);
    
    return g_test_failed > 0 ? 1 : 0;
}