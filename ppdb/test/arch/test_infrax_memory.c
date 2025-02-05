#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "internal/arch/PpxArch.h"
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
    InfraxMemory* memory = InfraxMemory_CLASS.new(&config);
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
    InfraxMemory_CLASS.free(memory);
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

    InfraxMemory* memory = InfraxMemory_CLASS.new(&config);
    assert(memory != NULL);
    printf("Memory instance created successfully\n");
    
    // 测试单个分配
    printf("Testing single allocation...\n");
    char* str = (char*)memory->alloc(memory, 100);
    assert(str != NULL);
    printf("Single allocation successful\n");
    
    printf("Testing string operations...\n");
    strcpy(str, "Hello, Pool!");
    assert(strcmp(str, "Hello, Pool!") == 0);
    printf("String operations successful\n");
    
    // 测试重分配
    printf("Testing reallocation...\n");
    str = (char*)memory->realloc(memory, str, 200);
    assert(str != NULL);
    assert(strcmp(str, "Hello, Pool!") == 0);
    printf("Reallocation successful\n");
    
    // 释放第一个分配
    printf("Testing deallocation...\n");
    memory->dealloc(memory, str);
    printf("Deallocation successful\n");
    
    // 测试多个小块分配
    printf("Testing multiple small allocations...\n");
    void* blocks[10];
    for (int i = 0; i < 10; i++) {
        blocks[i] = memory->alloc(memory, 50);
        assert(blocks[i] != NULL);
        memset(blocks[i], i, 50);
        printf("Block %d allocated and initialized\n", i);
    }
    
    // 释放一半的块
    printf("Testing partial deallocation...\n");
    for (int i = 0; i < 5; i++) {
        memory->dealloc(memory, blocks[i]);
        printf("Block %d deallocated\n", i);
    }
    
    // 重新分配新的块
    printf("Testing new allocations after partial deallocation...\n");
    for (int i = 0; i < 5; i++) {
        blocks[i] = memory->alloc(memory, 50);
        assert(blocks[i] != NULL);
        memset(blocks[i], i + 100, 50);
        printf("New block %d allocated and initialized\n", i);
    }
    
    // 释放所有块
    printf("Testing full deallocation...\n");
    for (int i = 0; i < 10; i++) {
        memory->dealloc(memory, blocks[i]);
        printf("Block %d deallocated\n", i);
    }
    
    // 测试大块分配
    printf("Testing large block allocation...\n");
    void* large_block = memory->alloc(memory, 512 * 1024);  // 512KB
    assert(large_block != NULL);
    memset(large_block, 0xFF, 512 * 1024);
    printf("Large block allocated and initialized\n");
    
    // 检查内存统计
    printf("Checking memory statistics...\n");
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    assert(stats.total_allocations > 0);
    assert(stats.current_usage >= 512 * 1024);
    printf("Memory statistics verified\n");
    
    // 释放大块
    printf("Deallocating large block...\n");
    memory->dealloc(memory, large_block);
    printf("Large block deallocated\n");
    
    // 再次检查统计
    printf("Checking final memory statistics...\n");
    memory->get_stats(memory, &stats);
    printf("Current memory usage: %zu bytes\n", stats.current_usage);
    printf("Final memory statistics verified\n");
    
    // 清理
    printf("Cleaning up memory pool...\n");
    InfraxMemory_CLASS.free(memory);
}

// 测试GC内存
void test_gc_memory() {
    printf("\nTesting GC Memory...\n");
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 512 * 1024  // 512KB
    };
    
    InfraxMemory* memory = InfraxMemory_CLASS.new(&config);
    assert(memory != NULL);
    
    // 分配一些内存
    void* root = memory->alloc(memory, 1000);
    assert(root != NULL);
    
    // 分配更多内存触发GC
    for (int i = 0; i < 100; i++) {
        void* temp = memory->alloc(memory, 1000);
        assert(temp != NULL);
        
        // 不释放内存，让GC来处理
        if (i % 10 == 0) {
            memory->collect(memory);
        }
    }
    
    // 检查内存统计
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    printf("GC memory stats - Current usage: %zu bytes\n", stats.current_usage);
    
    // 清理
    InfraxMemory_CLASS.free(memory);
    printf("GC memory test completed\n");
}

int main(void) {
    printf("Starting memory tests...\n");
    
    test_base_memory();
    test_pool_memory();
    test_gc_memory();
    
    printf("All memory tests passed!\n");
    return 0;
}
