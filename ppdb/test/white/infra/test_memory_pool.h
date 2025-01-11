/*
 * test_memory_pool.h - Memory Pool Test Suite
 */

#ifndef TEST_MEMORY_POOL_H
#define TEST_MEMORY_POOL_H

#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_error.h"

// 初始化测试
void test_memory_pool_init_default(void);
void test_memory_pool_init_custom(void);
void test_memory_pool_init_invalid(void);
void test_memory_pool_init_duplicate(void);

// 内存分配测试
void test_memory_pool_basic_alloc_free(void);
void test_memory_pool_various_sizes(void);
void test_memory_pool_alignment(void);
void test_memory_pool_split_blocks(void);
void test_memory_pool_continuous_alloc_free(void);

// 碎片处理测试
void test_memory_pool_merge_adjacent(void);
void test_memory_pool_fragmentation_stats(void);
void test_memory_pool_defrag(void);
void test_memory_pool_random_stress(void);

// 统计功能测试
void test_memory_pool_usage_stats(void);
void test_memory_pool_peak_stats(void);
void test_memory_pool_alloc_count(void);
void test_memory_pool_utilization(void);

// 边界条件测试
void test_memory_pool_out_of_memory(void);
void test_memory_pool_max_alloc(void);
void test_memory_pool_min_alloc(void);
void test_memory_pool_invalid_params(void);
void test_memory_pool_edge_cases(void);

// 测试套件入口
int run_memory_pool_test_suite(void);

#endif // TEST_MEMORY_POOL_H 