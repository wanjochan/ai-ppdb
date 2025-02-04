#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "internal/infrax/InfraxMemory.h"

// 测试基础内存管理
void test_base_memory() {
    printf("\nTesting Base Memory Management...\n");
    
    InfraxMemory* memory = infrax_memory_new();
    assert(memory != NULL);
    
    // 配置为基础模式
    InfraxMemoryConfig config = {
        .mode = MEMORY_MODE_BASE
    };
    memory->set_config(memory, &config);
    
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
    memory->free(memory);
    printf("Base Memory tests passed\n");
}

// 测试内存池
void test_pool_memory() {
    printf("\nTesting Memory Pool...\n");
    
    printf("Creating memory instance...\n");
    InfraxMemory* memory = infrax_memory_new();
    assert(memory != NULL);
    printf("Memory instance created successfully\n");
    
    // 配置为内存池模式
    printf("Configuring memory pool...\n");
    InfraxMemoryConfig config = {
        .mode = MEMORY_MODE_POOL,
        .pool_config = {
            .initial_size = 1024 * 1024,  // 1MB
            .alignment = 8
        }
    };
    memory->set_config(memory, &config);
    printf("Memory pool configured successfully\n");
    
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
    
    // 重新分配这些块
    printf("Testing reallocation of freed blocks...\n");
    for (int i = 0; i < 5; i++) {
        blocks[i] = memory->alloc(memory, 50);
        assert(blocks[i] != NULL);
        memset(blocks[i], i + 100, 50);
        printf("Block %d reallocated and initialized\n", i);
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
    printf("Large block allocated successfully\n");
    
    printf("Initializing large block...\n");
    memset(large_block, 0xFF, 512 * 1024);
    printf("Large block initialized successfully\n");
    
    // 测试统计信息
    printf("Testing memory statistics...\n");
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    assert(stats.total_allocations > 0);
    assert(stats.current_usage > 0);
    printf("Memory statistics verified successfully\n");
    
    // 释放大块
    printf("Deallocating large block...\n");
    memory->dealloc(memory, large_block);
    printf("Large block deallocated successfully\n");
    
    // 重新检查统计信息
    printf("Verifying final memory state...\n");
    memory->get_stats(memory, &stats);
    assert(stats.current_usage == 0);  // 所有内存都已释放
    printf("Final memory state verified successfully\n");
    
    // 清理
    printf("Cleaning up...\n");
    memory->free(memory);
    printf("Memory Pool tests passed\n");
}

// 测试垃圾回收
void test_gc_memory() {
    printf("\nTesting Garbage Collection...\n");
    
    InfraxMemory* memory = infrax_memory_new();
    assert(memory != NULL);
    
    // 配置为GC模式
    InfraxMemoryConfig config = {
        .mode = MEMORY_MODE_GC,
        .gc_config = {
            .heap_size = 1024 * 1024,  // 1MB
            .collection_threshold = 512 * 1024  // 512KB
        }
    };
    memory->set_config(memory, &config);
    
    // 分配一些对象
    void* root = memory->alloc(memory, 1000);
    assert(root != NULL);
    
    // 创建一些垃圾对象
    for (int i = 0; i < 100; i++) {
        void* temp = memory->alloc(memory, 1000);
        assert(temp != NULL);
        // 不保留引用，这些对象将成为垃圾
    }
    
    // 获取GC统计信息
    InfraxMemoryStats stats;
    memory->get_stats(memory, &stats);
    assert(stats.total_allocations > 100);
    
    // 清理
    memory->dealloc(memory, root);
    memory->free(memory);
    printf("Garbage Collection tests passed\n");
}

// 测试内存模式切换
void test_memory_mode_switch() {
    printf("\nTesting Memory Mode Switching...\n");
    
    InfraxMemory* memory = infrax_memory_new();
    assert(memory != NULL);
    
    // 基础模式 -> 内存池模式
    InfraxMemoryConfig config = {
        .mode = MEMORY_MODE_POOL,
        .pool_config = {
            .initial_size = 1024 * 1024,
            .alignment = 8
        }
    };
    memory->set_config(memory, &config);
    
    void* ptr1 = memory->alloc(memory, 100);
    assert(ptr1 != NULL);
    memory->dealloc(memory, ptr1);
    
    // 内存池模式 -> GC模式
    config.mode = MEMORY_MODE_GC;
    config.gc_config.heap_size = 1024 * 1024;
    config.gc_config.collection_threshold = 512 * 1024;
    memory->set_config(memory, &config);
    
    void* ptr2 = memory->alloc(memory, 100);
    assert(ptr2 != NULL);
    memory->dealloc(memory, ptr2);
    
    // GC模式 -> 基础模式
    config.mode = MEMORY_MODE_BASE;
    memory->set_config(memory, &config);
    
    void* ptr3 = memory->alloc(memory, 100);
    assert(ptr3 != NULL);
    memory->dealloc(memory, ptr3);
    
    // 清理
    memory->free(memory);
    printf("Memory Mode Switching tests passed\n");
}

int main() {
    printf("Starting Memory Management Tests...\n");
    
    test_base_memory();
    test_pool_memory();
    test_gc_memory();
    test_memory_mode_switch();
    
    printf("\nAll Memory Management tests passed!\n");
    return 0;
}
