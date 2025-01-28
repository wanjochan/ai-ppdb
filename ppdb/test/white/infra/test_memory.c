#include "internal/infra/infra_core.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_memory.h"
#include "../framework/test_framework.h"

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

// 测试初始化
static void test_memory_init(void) {
    // 确保系统未初始化
    TEST_ASSERT(!infra_is_initialized(INFRA_INIT_MEMORY));

    // 初始化默认配置
    infra_config_t config;
    infra_error_t err = infra_config_init(&config);
    TEST_ASSERT(err == INFRA_OK);

    // 修改内存配置
    config.memory.use_memory_pool = false;  // 使用系统内存
    config.memory.pool_initial_size = 1024 * 1024;  // 1MB
    config.memory.pool_alignment = sizeof(void*);

    // 初始化infra系统
    err = infra_init_with_config((infra_init_flags_t)INFRA_INIT_MEMORY, &config);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(infra_is_initialized(INFRA_INIT_MEMORY));
}

// 测试清理
static void test_memory_cleanup(void) {
    infra_cleanup();
    TEST_ASSERT(!infra_is_initialized(INFRA_INIT_MEMORY));
}

// 基本内存分配测试
static void test_memory_basic(void) {
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
}

// 内存操作测试
static void test_memory_operations(void) {
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
}

// 内存性能测试
static void test_memory_performance(void) {
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
}

// 内存压力测试
static void test_memory_stress(void) {
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
}

// 内存保护测试
static void test_memory_protection(void) {
    // 分配一页内存
    const size_t page_size = 4096;
    void* ptr = infra_mem_map(NULL, page_size, INFRA_PROT_READ | INFRA_PROT_WRITE);
    TEST_ASSERT(ptr != NULL);

    // 测试只读保护
    TEST_ASSERT(infra_mem_protect(ptr, page_size, INFRA_PROT_READ) == INFRA_OK);
    
    // 测试读写保护
    TEST_ASSERT(infra_mem_protect(ptr, page_size, INFRA_PROT_READ | INFRA_PROT_WRITE) == INFRA_OK);
    
    // 测试执行保护
    TEST_ASSERT(infra_mem_protect(ptr, page_size, INFRA_PROT_READ | INFRA_PROT_EXEC) == INFRA_OK);
    
    // 测试无访问权限
    TEST_ASSERT(infra_mem_protect(ptr, page_size, INFRA_PROT_NONE) == INFRA_OK);
    
    // 测试错误情况
    TEST_ASSERT(infra_mem_protect(NULL, page_size, INFRA_PROT_READ) == INFRA_ERROR_INVALID_PARAM);
    TEST_ASSERT(infra_mem_protect(ptr, 0, INFRA_PROT_READ) == INFRA_ERROR_INVALID_PARAM);
    
    // 清理
    TEST_ASSERT(infra_mem_unmap(ptr, page_size) == INFRA_OK);
}

int main(void) {
    // 禁用自动初始化
    setenv("INFRA_NO_AUTO_INIT", "1", 1);

    // 确保系统未初始化
    infra_cleanup();

    TEST_BEGIN();

    RUN_TEST(test_memory_init);
    RUN_TEST(test_memory_basic);
    RUN_TEST(test_memory_operations);
    RUN_TEST(test_memory_performance);
    RUN_TEST(test_memory_stress);
    RUN_TEST(test_memory_protection);
    RUN_TEST(test_memory_cleanup);

    TEST_END();

    return 0;
} 