#include "framework/test_framework.h"
#include "internal/infra/infra_gc.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"

// 测试函数声明
static void test_gc_init(void);
static void test_gc_alloc(void);
static void test_gc_lifecycle(void);
static void test_gc_stress(void);

// 打印GC统计信息
static void print_gc_stats(const char* prefix) {
    infra_gc_stats_t stats;
    infra_gc_get_stats(&stats);
    printf("%s: total_allocated=%zu, current_allocated=%zu, total_freed=%zu, collections=%zu\n",
           prefix, stats.total_allocated, stats.current_allocated, 
           stats.total_freed, stats.total_collections);
}

// 测试GC初始化
static void test_gc_init(void) {
    // 使用较小的阈值以便更容易触发GC
    infra_gc_config_t config = {
        .initial_heap_size = 1024,
        .gc_threshold = 512,  // 较小的阈值
        .enable_debug = true
    };
    
    infra_error_t err = infra_gc_init_with_stack(&config, &config);
    TEST_ASSERT(err == INFRA_OK);
    
    infra_gc_stats_t stats;
    infra_gc_get_stats(&stats);
    
    TEST_ASSERT(stats.total_allocated == 0);
    TEST_ASSERT(stats.current_allocated == 0);
    TEST_ASSERT(stats.total_collections == 0);
}

// 测试基本内存分配
static void test_gc_alloc(void) {
    printf("\n=== Starting test_gc_alloc ===\n");
    
    // 使用较小的阈值
    infra_gc_config_t config = {
        .initial_heap_size = 1024,
        .gc_threshold = 512,
        .enable_debug = true
    };
    infra_gc_init_with_stack(&config, &config);
    
    print_gc_stats("After init");
    
    // 分配一些内存并保持引用
    char* str = infra_gc_alloc(100);
    TEST_ASSERT(str != NULL);
    
    // 写入数据
    strcpy(str, "Hello World");
    TEST_ASSERT(strcmp(str, "Hello World") == 0);
    
    print_gc_stats("After first allocation");
    
    // 分配更多内存触发GC
    for (int i = 0; i < 20; i++) {
        void* ptr = infra_gc_alloc(50);  // 每次分配50字节
        TEST_ASSERT(ptr != NULL);
        // 不保持引用，让这些对象成为垃圾
        
        if (i % 5 == 0) {
            print_gc_stats("During allocations");
        }
    }
    
    // 强制进行GC
    infra_gc_collect();
    print_gc_stats("After forced GC");
    
    // 验证str的内容没有被GC破坏
    TEST_ASSERT(strcmp(str, "Hello World") == 0);
    
    infra_gc_stats_t stats;
    infra_gc_get_stats(&stats);
    TEST_ASSERT(stats.total_collections > 0);
    TEST_ASSERT(stats.total_freed > 0);
    
    printf("=== Finished test_gc_alloc ===\n\n");
}

// 测试对象生命周期
static void test_gc_lifecycle(void) {
    printf("\n=== Starting test_gc_lifecycle ===\n");
    
    // 使用较小的阈值
    infra_gc_config_t config = {
        .initial_heap_size = 1024,
        .gc_threshold = 512,
        .enable_debug = true
    };
    infra_gc_init_with_stack(&config, &config);
    
    print_gc_stats("After init");
    
    // 创建一些相互引用的对象
    typedef struct node {
        struct node* next;
        int data;
    } node_t;
    
    // 创建循环链表
    node_t* head = infra_gc_alloc(sizeof(node_t));
    TEST_ASSERT(head != NULL);
    head->data = 1;
    
    node_t* second = infra_gc_alloc(sizeof(node_t));
    TEST_ASSERT(second != NULL);
    second->data = 2;
    
    head->next = second;
    second->next = head;
    
    print_gc_stats("After creating nodes");
    
    // 让对象不可达
    head = NULL;
    second = NULL;
    
    // 强制GC
    infra_gc_collect();
    print_gc_stats("After GC");
    
    infra_gc_stats_t stats;
    infra_gc_get_stats(&stats);
    TEST_ASSERT(stats.total_collections > 0);
    TEST_ASSERT(stats.total_freed > 0);
    
    printf("=== Finished test_gc_lifecycle ===\n\n");
}

// 测试大规模分配
static void test_gc_stress(void) {
    printf("\n=== Starting test_gc_stress ===\n");
    
    // 使用较小的阈值
    infra_gc_config_t config = {
        .initial_heap_size = 1024,
        .gc_threshold = 512,
        .enable_debug = true
    };
    infra_gc_init_with_stack(&config, &config);
    
    print_gc_stats("After init");
    
    const int NUM_ALLOCS = 100;  // 减少分配次数，但每次分配更大
    void* ptrs[10];  // 保持一些对象存活
    
    // 大量分配
    for (int i = 0; i < NUM_ALLOCS; i++) {
        void* ptr = infra_gc_alloc(50);  // 固定大小以确保触发GC
        TEST_ASSERT(ptr != NULL);
        
        // 保留一些对象
        if (i % 10 == 0) {
            ptrs[i/10] = ptr;
            // 写入一些数据
            strcpy((char*)ptr, "test data");
        }
        
        if (i % 20 == 0) {
            print_gc_stats("During allocations");
        }
    }
    
    print_gc_stats("After all allocations");
    
    // 强制GC
    infra_gc_collect();
    print_gc_stats("After forced GC");
    
    infra_gc_stats_t stats;
    infra_gc_get_stats(&stats);
    
    // 验证GC确实发生了
    TEST_ASSERT(stats.total_collections > 0);
    TEST_ASSERT(stats.total_freed > 0);
    
    // 验证保留的对象还在且数据完整
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(ptrs[i] != NULL);
        TEST_ASSERT(strcmp((char*)ptrs[i], "test data") == 0);
    }
    
    printf("=== Finished test_gc_stress ===\n\n");
}

// 测试入口
int main(void) {
    TEST_BEGIN();
    RUN_TEST(test_gc_init);
    RUN_TEST(test_gc_alloc);
    RUN_TEST(test_gc_lifecycle);
    RUN_TEST(test_gc_stress);
    TEST_END();
}
