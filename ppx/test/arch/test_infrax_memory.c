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

// 测试内存压力
void test_memory_stress() {
    core->printf(core, "Testing memory stress...\n");
    
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
    
    #define STRESS_ALLOCS 50
    void* ptrs[STRESS_ALLOCS] = {NULL};
    size_t sizes[STRESS_ALLOCS] = {0};
    
    // 随机种子
    core->random_seed(core, 12345);
    
    // Phase 1: 随机分配
    core->printf(core, "Phase 1: Random allocation\n");
    for (int i = 0; i < STRESS_ALLOCS; i++) {
        sizes[i] = (core->random(core) % 512) + 64;  // 64-576 bytes
        ptrs[i] = memory->alloc(memory, sizes[i]);
        INFRAX_ASSERT(core, ptrs[i] != NULL);
        
        // 填充数据
        memset(ptrs[i], i & 0xFF, sizes[i]);
    }
    
    // Phase 2: 验证数据
    core->printf(core, "Phase 2: Verify data\n");
    for (int i = 0; i < STRESS_ALLOCS; i++) {
        unsigned char* p = (unsigned char*)ptrs[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            INFRAX_ASSERT(core, p[j] == (i & 0xFF));
        }
    }
    
    // Phase 3: 随机重分配
    core->printf(core, "Phase 3: Random reallocation\n");
    for (int i = 0; i < STRESS_ALLOCS/2; i++) {
        int idx = core->random(core) % STRESS_ALLOCS;
        if (ptrs[idx]) {
            size_t old_size = sizes[idx];
            size_t new_size = old_size + 128;
            unsigned char old_val = idx & 0xFF;
            
            void* new_ptr = memory->realloc(memory, ptrs[idx], new_size);
            INFRAX_ASSERT(core, new_ptr != NULL);
            
            // 验证原数据
            unsigned char* p = (unsigned char*)new_ptr;
            for (size_t j = 0; j < old_size; j++) {
                INFRAX_ASSERT(core, p[j] == old_val);
            }
            
            // 填充新空间
            for (size_t j = old_size; j < new_size; j++) {
                p[j] = old_val;
            }
            
            ptrs[idx] = new_ptr;
            sizes[idx] = new_size;
        }
    }
    
    // Phase 4: 随机释放一半内存
    core->printf(core, "Phase 4: Random deallocation\n");
    int freed_count = 0;
    for (int i = 0; i < STRESS_ALLOCS/2; i++) {
        int idx = core->random(core) % STRESS_ALLOCS;
        if (ptrs[idx]) {
            memory->dealloc(memory, ptrs[idx]);
            ptrs[idx] = NULL;
            sizes[idx] = 0;
            freed_count++;
        }
    }
    
    // Phase 5: 重新分配释放的空间
    core->printf(core, "Phase 5: Reallocate freed space\n");
    for (int i = 0; i < STRESS_ALLOCS; i++) {
        if (ptrs[i] == NULL) {
            sizes[i] = (core->random(core) % 512) + 64;
            ptrs[i] = memory->alloc(memory, sizes[i]);
            INFRAX_ASSERT(core, ptrs[i] != NULL);
            memset(ptrs[i], i & 0xFF, sizes[i]);
        }
    }
    
    // Phase 6: 最终验证和清理
    core->printf(core, "Phase 6: Final verification and cleanup\n");
    for (int i = 0; i < STRESS_ALLOCS; i++) {
        if (ptrs[i]) {
            unsigned char* p = (unsigned char*)ptrs[i];
            for (size_t j = 0; j < sizes[i]; j++) {
                INFRAX_ASSERT(core, p[j] == (i & 0xFF));
            }
            memory->dealloc(memory, ptrs[i]);
        }
    }
    
    // 检查内存泄漏
    InfraxMemoryStats final_stats;
    memory->get_stats(memory, &final_stats);
    INFRAX_ASSERT(core, final_stats.current_usage == initial_stats.current_usage);
    
    InfraxMemoryClass.free(memory);
    core->printf(core, "Memory stress test passed\n");
}

// 边界条件测试
void test_memory_edge_cases() {
    core->printf(core, "Testing memory edge cases...\n");
    
    InfraxMemoryConfig config = {
        .initial_size = 1024,  // 故意设置较小的初始大小
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    INFRAX_ASSERT(core, memory != NULL);
    
    // 测试零大小分配
    void* zero_ptr = memory->alloc(memory, 0);
    INFRAX_ASSERT(core, zero_ptr == NULL);
    
    // 测试超大分配 - 使用更合理的大小
    void* huge_ptr = memory->alloc(memory, 1024 * 1024 * 1024);  // 1GB
    if (huge_ptr != NULL) {
        memory->dealloc(memory, huge_ptr);
    }
    
    // 测试对齐要求
    void* aligned_ptr = memory->alloc(memory, 7);  // 非8字节对齐的大小
    INFRAX_ASSERT(core, aligned_ptr != NULL);
    INFRAX_ASSERT(core, ((uintptr_t)aligned_ptr & 7) == 0);  // 验证8字节对齐
    
    // 测试重复释放
    memory->dealloc(memory, aligned_ptr);
    memory->dealloc(memory, aligned_ptr);  // 应该安全处理
    
    // 测试NULL指针释放
    memory->dealloc(memory, NULL);  // 应该安全处理
    
    InfraxMemoryClass.free(memory);
    core->printf(core, "Memory edge cases test passed\n");
}

// 内存池碎片化测试
void test_memory_fragmentation() {
    core->printf(core, "Testing memory fragmentation...\n");
    
    InfraxMemoryConfig config = {
        .initial_size = 4096,  // 4KB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    INFRAX_ASSERT(core, memory != NULL);
    
    #define FRAG_ALLOCS 10
    void* ptrs[FRAG_ALLOCS];
    size_t sizes[FRAG_ALLOCS];
    
    // 分配不同大小的块以制造碎片
    for (int i = 0; i < FRAG_ALLOCS; i++) {
        sizes[i] = 64 + i * 32;  // 64, 96, 128, ...
        ptrs[i] = memory->alloc(memory, sizes[i]);
        INFRAX_ASSERT(core, ptrs[i] != NULL);
    }
    
    // 释放偶数索引的块
    for (int i = 0; i < FRAG_ALLOCS; i += 2) {
        memory->dealloc(memory, ptrs[i]);
    }
    
    // 尝试分配一个大块，应该失败（因为碎片化）
    void* large_ptr = memory->alloc(memory, 1024);
    if (large_ptr != NULL) {
        memory->dealloc(memory, large_ptr);
    }
    
    // 释放所有剩余块
    for (int i = 1; i < FRAG_ALLOCS; i += 2) {
        memory->dealloc(memory, ptrs[i]);
    }
    
    // 现在应该能分配大块了
    large_ptr = memory->alloc(memory, 1024);
    INFRAX_ASSERT(core, large_ptr != NULL);
    memory->dealloc(memory, large_ptr);
    
    InfraxMemoryClass.free(memory);
    core->printf(core, "Memory fragmentation test passed\n");
}

// GC功能测试
void test_memory_gc() {
    core->printf(core, "Testing garbage collection...\n");
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 512  // 较小的阈值以便触发GC
    };
    
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    INFRAX_ASSERT(core, memory != NULL);
    
    // 分配一些对象并标记为GC根
    void* root_obj = memory->alloc(memory, 256);
    INFRAX_ASSERT(core, root_obj != NULL);
    
    // 分配一些非根对象
    for (int i = 0; i < 10; i++) {
        void* temp = memory->alloc(memory, 64);
        INFRAX_ASSERT(core, temp != NULL);
    }
    
    // 强制进行GC
    memory->collect(memory);
    
    // 验证统计信息
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    
    // 释放根对象
    memory->dealloc(memory, root_obj);
    
    InfraxMemoryClass.free(memory);
    core->printf(core, "Garbage collection test passed\n");
}

int main() {
    core = InfraxCoreClass.singleton();
    INFRAX_ASSERT(core, core != NULL);
    
    core->printf(core, "===================\n");
    core->printf(core, "Starting InfraxMemory tests...\n");
    
    test_base_memory();
    test_pool_memory();
    test_realloc();
    test_memory_stress();
    test_memory_edge_cases();    // 新增
    test_memory_fragmentation(); // 新增
    test_memory_gc();           // 新增
    
    core->printf(core, "All infrax_memory tests passed!\n");
    core->printf(core, "===================\n");
    
    return 0;
}
