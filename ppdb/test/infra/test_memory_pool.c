#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_error.h"
#include <assert.h>
#include <stdio.h>

// 测试配置
static void test_memory_init(void) {
    printf("Testing memory initialization...\n");
    
    // 测试无效参数
    assert(infra_memory_init(NULL) == INFRA_ERROR_INVALID_PARAM);
    
    // 测试无效配置
    infra_memory_config_t invalid_config = {
        .use_memory_pool = true,
        .pool_initial_size = 0,
        .pool_alignment = 8
    };
    assert(infra_memory_init(&invalid_config) == INFRA_ERROR_INVALID_PARAM);
    
    // 测试有效配置
    infra_memory_config_t valid_config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = 8
    };
    assert(infra_memory_init(&valid_config) == INFRA_OK);
    
    infra_memory_cleanup();
    printf("Memory initialization tests passed.\n");
}

// 测试基本分配
static void test_basic_allocation(void) {
    printf("Testing basic memory allocation...\n");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    assert(infra_memory_init(&config) == INFRA_OK);
    
    // 测试malloc
    void* ptr1 = infra_malloc(100);
    assert(ptr1 != NULL);
    
    // 测试calloc
    void* ptr2 = infra_calloc(10, 10);
    assert(ptr2 != NULL);
    
    // 验证calloc初始化为0
    char* data = (char*)ptr2;
    for (int i = 0; i < 100; i++) {
        assert(data[i] == 0);
    }
    
    // 测试realloc
    void* ptr3 = infra_realloc(ptr1, 200);
    assert(ptr3 != NULL);
    
    // 清理
    infra_free(ptr2);
    infra_free(ptr3);
    
    infra_memory_cleanup();
    printf("Basic allocation tests passed.\n");
}

// 测试内存对齐
static void test_alignment(void) {
    printf("Testing memory alignment...\n");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    assert(infra_memory_init(&config) == INFRA_OK);
    
    // 分配不同大小的内存，检查对齐
    for (size_t size = 1; size <= 1024; size *= 2) {
        void* ptr = infra_malloc(size);
        assert(ptr != NULL);
        assert(((uintptr_t)ptr % 8) == 0);
        infra_free(ptr);
    }
    
    infra_memory_cleanup();
    printf("Alignment tests passed.\n");
}

// 测试内存统计
static void test_memory_stats(void) {
    printf("Testing memory statistics...\n");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    assert(infra_memory_init(&config) == INFRA_OK);
    
    infra_memory_stats_t stats;
    assert(infra_memory_get_stats(&stats) == INFRA_OK);
    
    // 初始状态
    assert(stats.current_usage == 0);
    assert(stats.total_allocations == 0);
    
    // 分配内存后
    void* ptr = infra_malloc(1000);
    assert(ptr != NULL);
    
    assert(infra_memory_get_stats(&stats) == INFRA_OK);
    assert(stats.current_usage > 0);
    assert(stats.total_allocations == 1);
    assert(stats.pool_utilization > 0);
    
    // 释放内存后
    infra_free(ptr);
    
    infra_memory_cleanup();
    printf("Memory statistics tests passed.\n");
}

// 测试内存碎片化
static void test_fragmentation(void) {
    printf("Testing memory fragmentation...\n");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    assert(infra_memory_init(&config) == INFRA_OK);
    
    // 创建碎片
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = infra_malloc(100);
        assert(ptrs[i] != NULL);
    }
    
    // 释放一些块，创建碎片
    for (int i = 0; i < 100; i += 2) {
        infra_free(ptrs[i]);
    }
    
    infra_memory_stats_t stats;
    assert(infra_memory_get_stats(&stats) == INFRA_OK);
    
    // 检查碎片率
    assert(stats.pool_fragmentation > 0);
    
    // 清理
    for (int i = 1; i < 100; i += 2) {
        infra_free(ptrs[i]);
    }
    
    infra_memory_cleanup();
    printf("Fragmentation tests passed.\n");
}

// 测试边界情况
static void test_edge_cases(void) {
    printf("Testing edge cases...\n");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024,
        .pool_alignment = 8
    };
    assert(infra_memory_init(&config) == INFRA_OK);
    
    // 测试零大小分配
    assert(infra_malloc(0) == NULL);
    assert(infra_calloc(0, 10) == NULL);
    assert(infra_calloc(10, 0) == NULL);
    
    // 测试大于池大小的分配
    void* ptr = infra_malloc(2048);
    assert(ptr != NULL);  // 应该从系统分配
    infra_free(ptr);
    
    // 测试NULL指针释放
    infra_free(NULL);  // 不应崩溃
    
    infra_memory_cleanup();
    printf("Edge case tests passed.\n");
}

int main(void) {
    printf("Starting memory pool tests...\n");
    
    test_memory_init();
    test_basic_allocation();
    test_alignment();
    test_memory_stats();
    test_fragmentation();
    test_edge_cases();
    
    printf("All memory pool tests passed!\n");
    return 0;
} 