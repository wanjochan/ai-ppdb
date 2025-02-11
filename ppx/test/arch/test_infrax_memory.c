#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxCore.h"
InfraxCore* core = NULL;
// 测试基础内存管理
void test_base_memory() {
    core->printf(core, "Testing base memory management...\n");
    
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
    core->strcpy(core, str, "Hello, Memory!");
    INFRAX_ASSERT(core, core->strcmp(core, str, "Hello, Memory!") == 0);
    
    // 测试重分配
    str = (char*)memory->realloc(memory, str, 200);
    INFRAX_ASSERT(core, str != NULL);
    INFRAX_ASSERT(core, core->strcmp(core, str, "Hello, Memory!") == 0);
    
    // 测试统计信息
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    INFRAX_ASSERT(core, stats.total_allocations > 0);
    INFRAX_ASSERT(core, stats.current_usage > 0);
    
    // 清理
    memory->dealloc(memory, str);
    InfraxMemoryClass.free(memory);
    
    core->printf(core, "Base memory management test passed\n");
}

// 测试内存池管理
void test_pool_memory() {
    core->printf(core, "Testing pool memory management...\n");
    
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
    
    core->printf(core, "Pool memory management test passed\n");
}

// 测试内存重分配
void test_realloc() {
    core->printf(core, "Testing memory reallocation...\n");
    
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
    char* ptr = (char*)memory->alloc(memory, 100);
    INFRAX_ASSERT(core, ptr != NULL);
    
    // 填充数据
    for (int i = 0; i < 100; i++) {
        ptr[i] = 'A';
    }
    
    // 重新分配更大的内存
    char* new_ptr = (char*)memory->realloc(memory, ptr, 200);
    INFRAX_ASSERT(core, new_ptr != NULL);
    
    // 验证原始数据保持不变
    for (int i = 0; i < 100; i++) {
        INFRAX_ASSERT(core, new_ptr[i] == 'A');
    }
    
    // 释放内存
    memory->dealloc(memory, new_ptr);
    
    // 测试内存管理器销毁
    InfraxMemoryClass.free(memory);
    
    core->printf(core, "Memory reallocation test passed\n");
}

// 主测试函数
int main() {
    if (core==NULL) core = InfraxCoreClass.singleton();
    core->printf(core, "===================\nStarting InfraxMemory tests...\n");
    
    test_base_memory();
    test_pool_memory();
    test_realloc();
    
    core->printf(core, "All infrax_memory tests passed!\n===================\n");
    return 0;
}
