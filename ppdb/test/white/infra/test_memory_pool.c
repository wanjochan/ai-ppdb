/*
 * test_memory_pool.c - Memory Pool Test Suite Implementation
 */

#include "test/white/infra/test_memory_pool.h"
#include "test/white/framework/test_framework.h"
#include "internal/infra/infra_core.h"

// 测试状态结构
typedef struct {
    const char* test_name;
    bool passed;
    char message[256];
} test_result_t;

// 全局测试状态
static struct {
    int total_tests;
    int passed_tests;
} g_test_state = {0};

// 测试辅助函数
static void log_test_start(const char* test_name) {
    infra_log(INFRA_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, 
              "[TEST START] %s\n", test_name);
}

static void log_test_end(const char* test_name, bool passed, const char* message) {
    const char* result = passed ? "PASSED" : "FAILED";
    infra_log(INFRA_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
              "[TEST END] %s: %s - %s\n", test_name, result, message);
    
    // 更新测试状态
    g_test_state.total_tests++;
    if (passed) {
        g_test_state.passed_tests++;
    }
}

static void test_setup(void) {
    g_test_state.total_tests = 0;
    g_test_state.passed_tests = 0;
    
    // 初始化基础设施层
    infra_config_t config;
    infra_config_init(&config);
    config.log.level = INFRA_LOG_LEVEL_INFO;
    config.log.log_file = "ppdb/ai/dev/logs/task_005.log";
    infra_init_with_config(INFRA_INIT_LOG, &config);
    
    infra_log(INFRA_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
              "\n=== Memory Pool Test Suite Started ===\n\n");
}

static void test_teardown(void) {
    infra_log(INFRA_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
              "\n=== Memory Pool Test Suite Completed ===\n");
    infra_log(INFRA_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
              "Total Tests: %d\n", g_test_state.total_tests);
    infra_log(INFRA_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
              "Passed Tests: %d\n", g_test_state.passed_tests);
    infra_log(INFRA_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__,
              "Success Rate: %.2f%%\n\n",
              (float)g_test_state.passed_tests / g_test_state.total_tests * 100.0f);
    
    infra_cleanup();
}

// 初始化测试实现
void test_memory_pool_init_default(void) {
    printf("\nRunning test: test_memory_pool_init_default\n");
    log_test_start("test_memory_pool_init_default");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    infra_memory_cleanup();
    log_test_end("test_memory_pool_init_default", true, "Default initialization test passed");
}

// 自定义配置初始化测试
void test_memory_pool_init_custom(void) {
    printf("\nRunning test: test_memory_pool_init_custom\n");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 2 * 1024 * 1024,  // 2MB
        .pool_alignment = 16
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 验证内存池状态
    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);
    
    infra_memory_cleanup();
}

// 无效参数测试
void test_memory_pool_init_invalid(void) {
    printf("\nRunning test: test_memory_pool_init_invalid\n");
    
    // 测试空配置
    infra_error_t err = infra_memory_init(NULL);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);
    infra_memory_cleanup();  // 清理状态
    
    // 测试无效大小
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 0,
        .pool_alignment = 8
    };
    
    err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);
    infra_memory_cleanup();  // 清理状态
    
    // 测试无效对齐
    config.pool_initial_size = 1024 * 1024;
    config.pool_alignment = 0;
    
    err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_ERROR_INVALID_PARAM);
    infra_memory_cleanup();  // 清理状态
}

// 重复初始化测试
void test_memory_pool_init_duplicate(void) {
    printf("\nRunning test: test_memory_pool_init_duplicate\n");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    // 第一次初始化
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 尝试重复初始化
    err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_ERROR_EXISTS);  // 修改期望的错误码
    
    infra_memory_cleanup();
}

// 基本内存分配测试
void test_memory_pool_basic_alloc(void) {
    printf("\nRunning test: test_memory_pool_basic_alloc\n");
    log_test_start("test_memory_pool_basic_alloc");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试基本分配
    void* ptr1 = infra_malloc(100);
    TEST_ASSERT(ptr1 != NULL);
    
    // 验证内存使用统计
    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage > 0);
    TEST_ASSERT(stats.total_allocations == 1);
    
    // 测试内存写入
    infra_memset(ptr1, 0x55, 100);
    
    // 释放内存
    infra_free(ptr1);
    
    // 验证释放后的统计
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);
    
    log_test_end("test_memory_pool_basic_alloc", true, "Basic allocation test passed");
    infra_memory_cleanup();
}

// 不同大小内存分配测试
void test_memory_pool_various_sizes(void) {
    printf("\nRunning test: test_memory_pool_various_sizes\n");
    log_test_start("test_memory_pool_various_sizes");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 分配不同大小的内存块
    void* ptr1 = infra_malloc(8);      // 最小分配
    void* ptr2 = infra_malloc(1024);   // 1KB
    void* ptr3 = infra_malloc(4096);   // 4KB
    void* ptr4 = infra_malloc(65536);  // 64KB
    
    TEST_ASSERT(ptr1 != NULL);
    TEST_ASSERT(ptr2 != NULL);
    TEST_ASSERT(ptr3 != NULL);
    TEST_ASSERT(ptr4 != NULL);
    
    // 验证内存对齐
    TEST_ASSERT(((uintptr_t)ptr1 & 7) == 0);
    TEST_ASSERT(((uintptr_t)ptr2 & 7) == 0);
    TEST_ASSERT(((uintptr_t)ptr3 & 7) == 0);
    TEST_ASSERT(((uintptr_t)ptr4 & 7) == 0);
    
    // 验证内存使用统计
    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.total_allocations == 4);
    
    // 释放内存
    infra_free(ptr1);
    infra_free(ptr2);
    infra_free(ptr3);
    infra_free(ptr4);
    
    log_test_end("test_memory_pool_various_sizes", true, "Various sizes allocation test passed");
    infra_memory_cleanup();
}

// 内存碎片测试
void test_memory_pool_fragmentation(void) {
    printf("\nRunning test: test_memory_pool_fragmentation\n");
    log_test_start("test_memory_pool_fragmentation");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    #define NUM_ALLOCS 100
    void* ptrs[NUM_ALLOCS];
    size_t sizes[NUM_ALLOCS];
    
    // 分配不同大小的内存块
    for (int i = 0; i < NUM_ALLOCS; i++) {
        sizes[i] = 32 + (rand() % 512);  // 32-544字节随机大小
        ptrs[i] = infra_malloc(sizes[i]);
        TEST_ASSERT(ptrs[i] != NULL);
        
        // 写入一些数据
        infra_memset(ptrs[i], i & 0xFF, sizes[i]);
    }
    
    // 随机释放一半的内存块
    for (int i = 0; i < NUM_ALLOCS / 2; i++) {
        int idx = rand() % NUM_ALLOCS;
        if (ptrs[idx] != NULL) {
            infra_free(ptrs[idx]);
            ptrs[idx] = NULL;
        }
    }
    
    // 获取碎片统计
    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.pool_fragmentation > 0);
    
    // 重新分配一些内存块
    for (int i = 0; i < NUM_ALLOCS; i++) {
        if (ptrs[i] == NULL) {
            ptrs[i] = infra_malloc(sizes[i]);
            TEST_ASSERT(ptrs[i] != NULL);
        }
    }
    
    // 释放所有内存
    for (int i = 0; i < NUM_ALLOCS; i++) {
        if (ptrs[i] != NULL) {
            infra_free(ptrs[i]);
        }
    }
    
    log_test_end("test_memory_pool_fragmentation", true, "Fragmentation test passed");
    infra_memory_cleanup();
}

// 相邻内存块合并测试
void test_memory_pool_merge_adjacent(void) {
    printf("\nRunning test: test_memory_pool_merge_adjacent\n");
    log_test_start("test_memory_pool_merge_adjacent");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 分配三个连续的内存块
    void* ptr1 = infra_malloc(1024);  // 1KB
    void* ptr2 = infra_malloc(2048);  // 2KB
    void* ptr3 = infra_malloc(4096);  // 4KB
    
    TEST_ASSERT(ptr1 != NULL);
    TEST_ASSERT(ptr2 != NULL);
    TEST_ASSERT(ptr3 != NULL);
    
    // 获取初始内存统计
    infra_memory_stats_t stats1;
    err = infra_memory_get_stats(&stats1);
    TEST_ASSERT(err == INFRA_OK);
    
    // 按特定顺序释放内存，触发合并
    infra_free(ptr2);  // 释放中间块
    
    // 获取释放一个块后的统计
    infra_memory_stats_t stats2;
    err = infra_memory_get_stats(&stats2);
    TEST_ASSERT(err == INFRA_OK);
    
    // 释放第一个块，应该触发向前合并
    infra_free(ptr1);
    
    // 获取释放两个块后的统计
    infra_memory_stats_t stats3;
    err = infra_memory_get_stats(&stats3);
    TEST_ASSERT(err == INFRA_OK);
    
    // 验证合并是否正确发生
    TEST_ASSERT(stats3.free_blocks < stats2.free_blocks);
    
    // 释放最后一个块，应该触发完全合并
    infra_free(ptr3);
    
    // 获取最终统计
    infra_memory_stats_t stats4;
    err = infra_memory_get_stats(&stats4);
    TEST_ASSERT(err == INFRA_OK);
    
    // 验证最终状态
    TEST_ASSERT(stats4.current_usage == 0);
    TEST_ASSERT(stats4.free_blocks == 1);  // 应该只有一个大的空闲块
    
    // 验证合并后的内存仍然可用
    void* ptr_large = infra_malloc(7168);  // 7KB (1KB + 2KB + 4KB)
    TEST_ASSERT(ptr_large != NULL);
    infra_free(ptr_large);
    
    log_test_end("test_memory_pool_merge_adjacent", true, "Adjacent blocks merge test passed");
    infra_memory_cleanup();
}

// 内存碎片统计测试
void test_memory_pool_fragmentation_stats(void) {
    printf("\nRunning test: test_memory_pool_fragmentation_stats\n");
    log_test_start("test_memory_pool_fragmentation_stats");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 获取初始状态统计
    infra_memory_stats_t initial_stats;
    err = infra_memory_get_stats(&initial_stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(initial_stats.fragmentation_ratio == 0.0f);  // 初始应该无碎片
    
    // 创建碎片模式：分配多个不同大小的块，然后释放部分块
    void* ptrs[10];
    size_t sizes[10] = {
        128,   // 128B
        256,   // 256B
        512,   // 512B
        1024,  // 1KB
        2048,  // 2KB
        4096,  // 4KB
        8192,  // 8KB
        16384, // 16KB
        32768, // 32KB
        65536  // 64KB
    };
    
    // 分配所有块
    for (int i = 0; i < 10; i++) {
        ptrs[i] = infra_malloc(sizes[i]);
        TEST_ASSERT(ptrs[i] != NULL);
    }
    
    // 获取全部分配后的统计
    infra_memory_stats_t full_stats;
    err = infra_memory_get_stats(&full_stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(full_stats.fragmentation_ratio <= 0.1f);  // 连续分配应该碎片率较低
    
    // 释放间隔的块以创建碎片
    for (int i = 0; i < 10; i += 2) {
        infra_free(ptrs[i]);
    }
    
    // 获取释放后的统计
    infra_memory_stats_t frag_stats;
    err = infra_memory_get_stats(&frag_stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(frag_stats.fragmentation_ratio > full_stats.fragmentation_ratio);
    TEST_ASSERT(frag_stats.free_blocks > 1);  // 应该有多个空闲块
    
    // 验证碎片统计的一致性
    size_t total_allocated = 0;
    for (int i = 1; i < 10; i += 2) {  // 计算剩余已分配块的大小
        total_allocated += sizes[i];
    }
    TEST_ASSERT(frag_stats.current_usage >= total_allocated);
    
    // 释放剩余的块
    for (int i = 1; i < 10; i += 2) {
        infra_free(ptrs[i]);
    }
    
    // 获取最终统计
    infra_memory_stats_t final_stats;
    err = infra_memory_get_stats(&final_stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(final_stats.current_usage == 0);
    TEST_ASSERT(final_stats.fragmentation_ratio == 0.0f);  // 全部释放后应该无碎片
    TEST_ASSERT(final_stats.free_blocks == 1);  // 应该合并为一个大块
    
    log_test_end("test_memory_pool_fragmentation_stats", true, "Fragmentation statistics test passed");
    infra_memory_cleanup();
}

// 内存碎片整理测试
void test_memory_pool_defrag(void) {
    printf("\nRunning test: test_memory_pool_defrag\n");
    log_test_start("test_memory_pool_defrag");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = 8,
        .enable_defrag = true  // 启用碎片整理功能
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 创建一个碎片化的场景
    #define NUM_BLOCKS 20
    void* ptrs[NUM_BLOCKS];
    size_t block_size = 1024;  // 1KB
    
    // 第一轮：分配所有块
    for (int i = 0; i < NUM_BLOCKS; i++) {
        ptrs[i] = infra_malloc(block_size);
        TEST_ASSERT(ptrs[i] != NULL);
        
        // 写入一些标记数据
        unsigned char* data = (unsigned char*)ptrs[i];
        for (size_t j = 0; j < block_size; j++) {
            data[j] = (unsigned char)i;
        }
    }
    
    // 释放一些块来创建碎片
    for (int i = 0; i < NUM_BLOCKS; i += 2) {
        infra_free(ptrs[i]);
        ptrs[i] = NULL;
    }
    
    // 获取碎片化后的统计
    infra_memory_stats_t before_defrag;
    err = infra_memory_get_stats(&before_defrag);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(before_defrag.fragmentation_ratio > 0.0f);
    
    // 执行碎片整理
    err = infra_memory_defrag();
    TEST_ASSERT(err == INFRA_OK);
    
    // 获取整理后的统计
    infra_memory_stats_t after_defrag;
    err = infra_memory_get_stats(&after_defrag);
    TEST_ASSERT(err == INFRA_OK);
    
    // 验证碎片整理的效果
    TEST_ASSERT(after_defrag.fragmentation_ratio < before_defrag.fragmentation_ratio);
    TEST_ASSERT(after_defrag.free_blocks <= before_defrag.free_blocks);
    
    // 验证现有数据的完整性
    for (int i = 1; i < NUM_BLOCKS; i += 2) {
        unsigned char* data = (unsigned char*)ptrs[i];
        for (size_t j = 0; j < block_size; j++) {
            TEST_ASSERT(data[j] == (unsigned char)i);
        }
    }
    
    // 尝试在碎片整理后分配大块内存
    size_t large_size = block_size * (NUM_BLOCKS / 2);  // 应该能容纳所有空闲空间
    void* large_ptr = infra_malloc(large_size);
    TEST_ASSERT(large_ptr != NULL);
    
    // 清理
    infra_free(large_ptr);
    for (int i = 1; i < NUM_BLOCKS; i += 2) {
        if (ptrs[i] != NULL) {
            infra_free(ptrs[i]);
        }
    }
    
    // 验证最终状态
    infra_memory_stats_t final_stats;
    err = infra_memory_get_stats(&final_stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(final_stats.current_usage == 0);
    TEST_ASSERT(final_stats.fragmentation_ratio == 0.0f);
    
    #undef NUM_BLOCKS
    
    log_test_end("test_memory_pool_defrag", true, "Memory defragmentation test passed");
    infra_memory_cleanup();
}

// 随机压力测试
void test_memory_pool_random_stress(void) {
    printf("\nRunning test: test_memory_pool_random_stress\n");
    log_test_start("test_memory_pool_random_stress");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 4 * 1024 * 1024,  // 4MB
        .pool_alignment = 8,
        .enable_defrag = true
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    #define MAX_ALLOCS 1000
    #define MIN_BLOCK_SIZE 32
    #define MAX_BLOCK_SIZE (64 * 1024)  // 64KB
    
    typedef struct {
        void* ptr;
        size_t size;
        bool in_use;
    } alloc_record_t;
    
    alloc_record_t allocs[MAX_ALLOCS] = {0};
    int total_allocs = 0;
    int active_allocs = 0;
    size_t total_allocated = 0;
    
    // 随机数种子
    srand((unsigned int)time(NULL));
    
    // 执行随机操作
    for (int i = 0; i < 5000; i++) {
        if (i % 100 == 0) {
            printf(".");  // 进度指示
            fflush(stdout);
        }
        
        // 随机选择操作：分配或释放
        bool do_alloc = (rand() % 100) < 60;  // 60%概率分配，40%概率释放
        
        if (do_alloc && total_allocs < MAX_ALLOCS) {
            // 随机大小分配
            size_t size = MIN_BLOCK_SIZE + (rand() % (MAX_BLOCK_SIZE - MIN_BLOCK_SIZE));
            void* ptr = infra_malloc(size);
            
            if (ptr != NULL) {
                // 记录分配
                int idx = total_allocs++;
                allocs[idx].ptr = ptr;
                allocs[idx].size = size;
                allocs[idx].in_use = true;
                active_allocs++;
                total_allocated += size;
                
                // 写入测试数据
                memset(ptr, (char)idx, size);
            }
        } else if (!do_alloc && active_allocs > 0) {
            // 随机选择一个活跃的分配来释放
            int attempts = 0;
            while (attempts < 10) {
                int idx = rand() % total_allocs;
                if (allocs[idx].in_use) {
                    // 验证内存内容
                    unsigned char* data = (unsigned char*)allocs[idx].ptr;
                    for (size_t j = 0; j < allocs[idx].size; j++) {
                        TEST_ASSERT(data[j] == (unsigned char)idx);
                    }
                    
                    // 释放内存
                    infra_free(allocs[idx].ptr);
                    allocs[idx].in_use = false;
                    active_allocs--;
                    total_allocated -= allocs[idx].size;
                    break;
                }
                attempts++;
            }
        }
        
        // 每500次操作执行一次碎片整理
        if (i % 500 == 499) {
            err = infra_memory_defrag();
            TEST_ASSERT(err == INFRA_OK);
            
            // 验证所有活跃内存块的内容
            for (int j = 0; j < total_allocs; j++) {
                if (allocs[j].in_use) {
                    unsigned char* data = (unsigned char*)allocs[j].ptr;
                    for (size_t k = 0; k < allocs[j].size; k++) {
                        TEST_ASSERT(data[k] == (unsigned char)j);
                    }
                }
            }
        }
        
        // 获取并验证内存统计
        infra_memory_stats_t stats;
        err = infra_memory_get_stats(&stats);
        TEST_ASSERT(err == INFRA_OK);
        TEST_ASSERT(stats.current_usage >= total_allocated);
    }
    
    printf("\n");  // 结束进度指示
    
    // 清理所有分配
    for (int i = 0; i < total_allocs; i++) {
        if (allocs[i].in_use) {
            infra_free(allocs[i].ptr);
        }
    }
    
    // 验证最终状态
    infra_memory_stats_t final_stats;
    err = infra_memory_get_stats(&final_stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(final_stats.current_usage == 0);
    TEST_ASSERT(final_stats.fragmentation_ratio == 0.0f);
    
    #undef MAX_ALLOCS
    #undef MIN_BLOCK_SIZE
    #undef MAX_BLOCK_SIZE
    
    log_test_end("test_memory_pool_random_stress", true, "Random stress test passed");
    infra_memory_cleanup();
}

// 压力测试
void test_memory_pool_stress(void) {
    printf("\nRunning test: test_memory_pool_stress\n");
    log_test_start("test_memory_pool_stress");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 4 * 1024 * 1024,  // 4MB
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    #define STRESS_ITERATIONS 1000
    #define MAX_ACTIVE_ALLOCS 200
    
    void* active_ptrs[MAX_ACTIVE_ALLOCS] = {NULL};
    size_t active_sizes[MAX_ACTIVE_ALLOCS] = {0};
    int active_count = 0;
    
    // 执行大量的随机分配和释放操作
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        if ((rand() % 100) < 60 && active_count < MAX_ACTIVE_ALLOCS) {
            // 60%概率进行分配
            size_t size = 16 + (rand() % 8192);  // 16B-8KB随机大小
            void* ptr = infra_malloc(size);
            if (ptr != NULL) {
                active_ptrs[active_count] = ptr;
                active_sizes[active_count] = size;
                active_count++;
                
                // 写入一些数据
                infra_memset(ptr, i & 0xFF, size);
            }
        } else if (active_count > 0) {
            // 40%概率进行释放
            int idx = rand() % active_count;
            infra_free(active_ptrs[idx]);
            
            // 移动最后一个活跃指针到当前位置
            active_count--;
            if (idx < active_count) {
                active_ptrs[idx] = active_ptrs[active_count];
                active_sizes[idx] = active_sizes[active_count];
            }
        }
        
        // 每100次操作检查一次内存统计
        if ((i % 100) == 0) {
            infra_memory_stats_t stats;
            err = infra_memory_get_stats(&stats);
            TEST_ASSERT(err == INFRA_OK);
            TEST_ASSERT(stats.current_usage <= config.pool_initial_size);
        }
    }
    
    // 释放所有剩余的内存
    for (int i = 0; i < active_count; i++) {
        infra_free(active_ptrs[i]);
    }
    
    // 最终检查
    infra_memory_stats_t stats;
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);
    
    log_test_end("test_memory_pool_stress", true, "Stress test passed");
    infra_memory_cleanup();
}

// 边界测试
void test_memory_pool_boundary(void) {
    printf("\nRunning test: test_memory_pool_boundary\n");
    log_test_start("test_memory_pool_boundary");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 64 * 1024,  // 64KB
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    // 测试最小分配
    void* ptr_min = infra_malloc(1);
    TEST_ASSERT(ptr_min != NULL);
    
    // 测试0大小分配
    void* ptr_zero = infra_malloc(0);
    TEST_ASSERT(ptr_zero == NULL);
    
    // 测试超大分配
    void* ptr_huge = infra_malloc(config.pool_initial_size + 1);
    TEST_ASSERT(ptr_huge == NULL);
    
    // 测试接近池大小的分配
    void* ptr_near = infra_malloc(config.pool_initial_size - 1024);
    TEST_ASSERT(ptr_near != NULL);
    
    // 测试剩余空间不足
    void* ptr_remain = infra_malloc(2048);
    TEST_ASSERT(ptr_remain == NULL);
    
    // 测试空指针释放
    infra_free(NULL);  // 应该安全返回
    
    // 释放有效内存
    infra_free(ptr_min);
    infra_free(ptr_near);
    
    log_test_end("test_memory_pool_boundary", true, "Boundary test passed");
    infra_memory_cleanup();
}

// 统计功能测试
void test_memory_pool_statistics(void) {
    printf("\nRunning test: test_memory_pool_statistics\n");
    log_test_start("test_memory_pool_statistics");
    
    infra_memory_config_t config = {
        .use_memory_pool = true,
        .pool_initial_size = 1024 * 1024,
        .pool_alignment = 8
    };
    
    infra_error_t err = infra_memory_init(&config);
    TEST_ASSERT(err == INFRA_OK);
    
    infra_memory_stats_t stats;
    
    // 初始状态检查
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);
    TEST_ASSERT(stats.peak_usage == 0);
    TEST_ASSERT(stats.total_allocations == 0);
    
    // 分配一些内存并检查统计
    void* ptr1 = infra_malloc(1024);
    TEST_ASSERT(ptr1 != NULL);
    
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage >= 1024);
    TEST_ASSERT(stats.peak_usage >= 1024);
    TEST_ASSERT(stats.total_allocations == 1);
    
    // 分配更大的内存并检查峰值
    void* ptr2 = infra_malloc(4096);
    TEST_ASSERT(ptr2 != NULL);
    
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage >= 5120);  // 1024 + 4096
    TEST_ASSERT(stats.peak_usage >= 5120);
    TEST_ASSERT(stats.total_allocations == 2);
    
    // 释放小块并检查统计变化
    infra_free(ptr1);
    
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage >= 4096);
    TEST_ASSERT(stats.peak_usage >= 5120);  // 峰值不应减少
    
    // 释放大块并检查统计
    infra_free(ptr2);
    
    err = infra_memory_get_stats(&stats);
    TEST_ASSERT(err == INFRA_OK);
    TEST_ASSERT(stats.current_usage == 0);
    TEST_ASSERT(stats.peak_usage >= 5120);  // 峰值仍然保持
    TEST_ASSERT(stats.total_allocations == 2);
    
    // 检查内存池利用率
    TEST_ASSERT(stats.pool_utilization >= 0);
    TEST_ASSERT(stats.pool_utilization <= 100);
    
    log_test_end("test_memory_pool_statistics", true, "Statistics test passed");
    infra_memory_cleanup();
}

// 测试套件入口
int run_memory_pool_test_suite(void) {
    int result;
    
    test_init();
    test_setup();
    printf("\nRunning tests...\n");
    
    // 运行初始化测试组
    RUN_TEST(test_memory_pool_init_default);
    RUN_TEST(test_memory_pool_init_custom);
    RUN_TEST(test_memory_pool_init_invalid);
    RUN_TEST(test_memory_pool_init_duplicate);
    
    // 运行内存分配测试组
    RUN_TEST(test_memory_pool_basic_alloc);
    RUN_TEST(test_memory_pool_various_sizes);
    
    // 运行碎片和压力测试组
    RUN_TEST(test_memory_pool_fragmentation);
    RUN_TEST(test_memory_pool_stress);
    
    // 运行边界和统计测试组
    RUN_TEST(test_memory_pool_boundary);
    RUN_TEST(test_memory_pool_statistics);
    
    test_teardown();
    test_report();
    test_cleanup();
    result = g_test_stats[TEST_STATS_FAILED] ? 1 : 0;
    
    return result;
} 