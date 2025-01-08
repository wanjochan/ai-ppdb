#include <cosmopolitan.h>
#include "../test_framework.h"

// 测试基本内存分配
static int test_mem_basic(void) {
    // 分配内存
    void* ptr = malloc(1024);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // 写入数据
    memset(ptr, 0xAA, 1024);
    
    // 释放内存
    free(ptr);
    return 0;
}

// 测试内存重分配
static int test_mem_realloc(void) {
    // 初始分配
    void* ptr = malloc(1024);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // 写入数据
    memset(ptr, 0xBB, 1024);
    
    // 重新分配
    ptr = realloc(ptr, 2048);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // 验证原数据
    unsigned char* bytes = ptr;
    for (int i = 0; i < 1024; i++) {
        TEST_ASSERT_EQUALS(bytes[i], 0xBB);
    }
    
    // 释放内存
    free(ptr);
    return 0;
}

// 测试内存对齐
static int test_mem_aligned(void) {
    void* ptr = NULL;
    int ret = posix_memalign(&ptr, 64, 1024);
    TEST_ASSERT_EQUALS(ret, 0);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // 验证对齐
    uintptr_t addr = (uintptr_t)ptr;
    TEST_ASSERT_EQUALS(addr % 64, 0);
    
    // 释放内存
    free(ptr);
    return 0;
}

// 测试大内存分配
static int test_mem_large(void) {
    // 分配大块内存
    size_t size = 100 * 1024 * 1024; // 100MB
    void* ptr = malloc(size);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // 写入数据
    memset(ptr, 0xCC, size);
    
    // 释放内存
    free(ptr);
    return 0;
}

// 测试多次分配释放
static int test_mem_multiple(void) {
    void* ptrs[1000];
    
    // 多次分配
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = malloc(1024);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
        memset(ptrs[i], i & 0xFF, 1024);
    }
    
    // 多次释放
    for (int i = 0; i < 1000; i++) {
        free(ptrs[i]);
    }
    
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_mem_basic);
    TEST_RUN(test_mem_realloc);
    TEST_RUN(test_mem_aligned);
    TEST_RUN(test_mem_large);
    TEST_RUN(test_mem_multiple);
    
    TEST_REPORT();
    return 0;
}