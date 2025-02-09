#include <stdio.h>
// #include <assert.h> use assert from our core
#include <string.h>
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxCore.h"

// 测试基础内存管理
void test_base_memory() {
    printf("Testing base memory management...\n");
    
    // Get core instance
    InfraxCore* core = InfraxCoreClass.singleton();
    
    // 创建内存管理器
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    INFRAX_ASSERT(core, memory != NULL);
    
    // 测试分配和写入
    char* str = (char*)memory->alloc(memory, 100);
    INFRAX_ASSERT(core, str != NULL);
    strcpy(str, "Hello, Memory!");
    INFRAX_ASSERT(core, strcmp(str, "Hello, Memory!") == 0);
    
    // 测试重分配
    str = (char*)memory->realloc(memory, str, 200);
    INFRAX_ASSERT(core, str != NULL);
    INFRAX_ASSERT(core, strcmp(str, "Hello, Memory!") == 0);
    
    // 测试统计信息
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    INFRAX_ASSERT(core, stats.total_allocations > 0);
    INFRAX_ASSERT(core, stats.current_usage > 0);
    
    // 清理
    memory->dealloc(memory, str);
    InfraxMemoryClass.free(memory);
    
    printf("Base memory management test passed\n");
}

// 测试内存池管理
void test_pool_memory() {
    printf("Testing pool memory management...\n");
    
    // Get core instance
    InfraxCore* core = InfraxCoreClass.singleton();
    
    // 创建内存管理器
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    INFRAX_ASSERT(core, memory != NULL);
    
    // 测试多个内存池分配
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = memory->alloc(memory, 100);
        INFRAX_ASSERT(core, ptrs[i] != NULL);
    }
    
    // 测试内存池释放
    for (int i = 0; i < 100; i++) {
        memory->dealloc(memory, ptrs[i]);
    }
    
    // 测试内存管理器销毁
    InfraxMemoryClass.free(memory);
    
    printf("Pool memory management test passed\n");
}

// 测试内存重分配
void test_realloc() {
    printf("Testing memory reallocation...\n");
    
    // Get core instance
    InfraxCore* core = InfraxCoreClass.singleton();
    
    // 创建内存管理器
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    INFRAX_ASSERT(core, memory != NULL);
    
    // 分配初始内存
    void* ptr = memory->alloc(memory, 100);
    INFRAX_ASSERT(core, ptr != NULL);
    
    // 填充数据
    memset(ptr, 'A', 100);
    
    // 重新分配更大的内存
    void* new_ptr = memory->realloc(memory, ptr, 200);
    INFRAX_ASSERT(core, new_ptr != NULL);
    
    // 验证原始数据保持不变
    char* data = (char*)new_ptr;
    for (int i = 0; i < 100; i++) {
        INFRAX_ASSERT(core, data[i] == 'A');
    }
    
    // 释放内存
    memory->dealloc(memory, new_ptr);
    
    // 测试内存管理器销毁
    InfraxMemoryClass.free(memory);
    
    printf("Memory reallocation test passed\n");
}

// 主测试函数
int main() {
    printf("===================\nStarting InfraxMemory tests...\n");
    
    test_base_memory();
    test_pool_memory();
    test_realloc();
    
    printf("All InfraxMemory tests passed!\n===================\n");
    return 0;
}
