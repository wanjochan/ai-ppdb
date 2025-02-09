#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "internal/infrax/InfraxMemory.h"

// 测试基础内存管理
void test_base_memory() {
    printf("\nTesting Base Memory Management...\n");
    
    // 创建基础内存配置
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = false,
        .gc_threshold = 0
    };
    
    // 使用类接口创建实例
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    assert(memory != NULL);
    
    // 测试分配和写入
    char* str = (char*)memory->alloc(memory, 100);
    assert(str != NULL);
    strcpy(str, "Hello, Memory!");
    assert(strcmp(str, "Hello, Memory!") == 0);
    
    // 测试重分配
    str = (char*)memory->realloc(memory, str, 200);
    assert(str != NULL);
    assert(strcmp(str, "Hello, Memory!") == 0);
    
    // 测试统计信息
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    assert(stats.total_allocations > 0);
    assert(stats.current_usage > 0);
    
    // 清理
    memory->dealloc(memory, str);
    InfraxMemoryClass.free(memory);
}

// 测试内存池
void test_pool_memory() {
    printf("\nTesting Memory Pool...\n");
    
    printf("Creating memory instance...\n");
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };

    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    assert(memory != NULL);
    printf("Memory instance created successfully\n");
    
    printf("Testing allocations...\n");
    void* ptrs[10];
    size_t sizes[10] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192};
    
    // 测试多次分配
    for (int i = 0; i < 10; i++) {
        ptrs[i] = memory->alloc(memory, sizes[i]);
        assert(ptrs[i] != NULL);
        memset(ptrs[i], 'A' + i, sizes[i]);
    }
    
    // 验证内容
    printf("Verifying memory contents...\n");
    for (int i = 0; i < 10; i++) {
        char* ptr = (char*)ptrs[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            assert(ptr[j] == 'A' + i);
        }
    }
    
    // 测试统计信息
    printf("Checking memory stats...\n");
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    assert(stats.total_allocations >= 10);
    assert(stats.current_usage > 0);
    
    // 释放一半的内存
    printf("Testing deallocations...\n");
    for (int i = 0; i < 5; i++) {
        memory->dealloc(memory, ptrs[i]);
    }
    
    // 重新分配
    printf("Testing reallocations...\n");
    for (int i = 0; i < 5; i++) {
        ptrs[i] = memory->alloc(memory, sizes[i]);
        assert(ptrs[i] != NULL);
        memset(ptrs[i], 'a' + i, sizes[i]);
    }
    
    // 验证新内容
    printf("Verifying new memory contents...\n");
    for (int i = 0; i < 5; i++) {
        char* ptr = (char*)ptrs[i];
        for (size_t j = 0; j < sizes[i]; j++) {
            assert(ptr[j] == 'a' + i);
        }
    }
    
    // 释放所有内存
    printf("Final cleanup...\n");
    for (int i = 0; i < 10; i++) {
        memory->dealloc(memory, ptrs[i]);
    }
    
    InfraxMemoryClass.free(memory);
}

// 测试垃圾回收
void test_gc_memory() {
    printf("\nTesting GC Memory...\n");
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 512 * 1024  // 512KB
    };
    
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    assert(memory != NULL);
    
    // 分配一些对象
    void* ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = memory->alloc(memory, 1024);
        assert(ptrs[i] != NULL);
    }
    
    // 触发GC
    memory->collect(memory);
    
    // 检查统计信息
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    assert(stats.total_allocations >= 5);
    
    // 清理
    for (int i = 0; i < 5; i++) {
        memory->dealloc(memory, ptrs[i]);
    }
    
    InfraxMemoryClass.free(memory);
}

int main(void) {
    printf("===================\nStarting Memory Tests...\n");
    
    test_base_memory();
    test_pool_memory();
    test_gc_memory();
    
    printf("\nAll Memory Tests Passed!\n");
    return 0;
}
