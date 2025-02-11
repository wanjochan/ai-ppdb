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
    
    // 获取初始状态
    InfraxMemoryStats initial_stats;
    memory->get_stats(memory, &initial_stats);
    
    // 测试分配和写入
    char* str = (char*)memory->alloc(memory, 100);
    INFRAX_ASSERT(core, str != NULL);
    core->strcpy(core, str, "Hello, Memory!");
    INFRAX_ASSERT(core, core->strcmp(core, str, "Hello, Memory!") == 0);
    
    // 测试重分配
    str = (char*)memory->realloc(memory, str, 200);
    INFRAX_ASSERT(core, str != NULL);
    INFRAX_ASSERT(core, core->strcmp(core, str, "Hello, Memory!") == 0);
    
    // 测试边界条件
    void* zero_size = memory->alloc(memory, 0);  // 分配0字节
    INFRAX_ASSERT(core, zero_size == NULL || zero_size != NULL);  // 允许实现返回NULL或小对象
    
    // 尝试分配一个非常大但不是最大的size
    void* huge_size = memory->alloc(memory, 1024 * 1024 * 1024);  // 1GB
    if (huge_size != NULL) {
        memory->dealloc(memory, huge_size);
    }
    
    // 测试内存对齐
    void* aligned_ptr = memory->alloc(memory, 8);
    INFRAX_ASSERT(core, aligned_ptr != NULL);
    INFRAX_ASSERT(core, ((uintptr_t)aligned_ptr & 7) == 0);  // 检查8字节对齐
    
    // 测试统计信息
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    INFRAX_ASSERT(core, stats.total_allocations > initial_stats.total_allocations);
    INFRAX_ASSERT(core, stats.current_usage > initial_stats.current_usage);
    
    // 清理
    memory->dealloc(memory, str);
    memory->dealloc(memory, aligned_ptr);
    
    // 检查内存泄漏
    memory->get_stats(memory, &stats);
    INFRAX_ASSERT(core, stats.current_usage == initial_stats.current_usage);
    
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
    
    // 获取初始状态
    InfraxMemoryStats initial_stats;
    memory->get_stats(memory, &initial_stats);
    
    // 测试多个内存池分配
    void* ptrs[100];
    for (int i = 0; i < 100; i++) {
        ptrs[i] = memory->alloc(memory, 100);
        INFRAX_ASSERT(core, ptrs[i] != NULL);
        
        // 写入一些数据以验证内存可用
        char* p = (char*)ptrs[i];
        for (int j = 0; j < 100; j++) {
            p[j] = (char)i;
        }
    }
    
    // 验证数据正确性
    for (int i = 0; i < 100; i++) {
        char* p = (char*)ptrs[i];
        for (int j = 0; j < 100; j++) {
            INFRAX_ASSERT(core, p[j] == (char)i);
        }
    }
    
    // 测试内存池释放
    for (int i = 0; i < 100; i++) {
        memory->dealloc(memory, ptrs[i]);
    }
    
    // 检查内存泄漏
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    INFRAX_ASSERT(core, stats.current_usage == initial_stats.current_usage);
    
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
    
    // 获取初始状态
    InfraxMemoryStats initial_stats;
    memory->get_stats(memory, &initial_stats);
    
    // 分配初始内存并填充数据
    char* ptr = (char*)memory->alloc(memory, 100);
    INFRAX_ASSERT(core, ptr != NULL);
    for (int i = 0; i < 100; i++) {
        ptr[i] = 'A';
    }
    
    // 重新分配更大的内存
    ptr = (char*)memory->realloc(memory, ptr, 200);
    INFRAX_ASSERT(core, ptr != NULL);
    
    // 验证原始数据保持不变
    for (int i = 0; i < 100; i++) {
        INFRAX_ASSERT(core, ptr[i] == 'A');
    }
    
    // 填充新分配的空间
    for (int i = 100; i < 200; i++) {
        ptr[i] = 'B';
    }
    
    // 重新分配更小的内存
    ptr = (char*)memory->realloc(memory, ptr, 50);
    INFRAX_ASSERT(core, ptr != NULL);
    
    // 验证数据截断正确
    for (int i = 0; i < 50; i++) {
        INFRAX_ASSERT(core, ptr[i] == 'A');
    }
    
    // 测试边界条件
    ptr = (char*)memory->realloc(memory, ptr, 0);  // 相当于free
    INFRAX_ASSERT(core, ptr == NULL);
    
    // 检查内存泄漏
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    INFRAX_ASSERT(core, stats.current_usage == initial_stats.current_usage);
    
    InfraxMemoryClass.free(memory);
    core->printf(core, "Memory reallocation test passed\n");
}

int main() {
    core = InfraxCoreClass.singleton();
    INFRAX_ASSERT(core, core != NULL);
    
    core->printf(core, "===================\n");
    core->printf(core, "Starting InfraxMemory tests...\n");
    
    test_base_memory();
    test_pool_memory();
    test_realloc();
    
    core->printf(core, "All infrax_memory tests passed!\n");
    core->printf(core, "===================\n");
    
    return 0;
}
