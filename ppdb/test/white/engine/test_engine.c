#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "../test_macros.h"

// 测试引擎初始化和销毁
int test_engine_init_destroy(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    
    // 初始化base
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));
    
    // 初始化engine
    ASSERT_OK(ppdb_engine_init(&engine, base));
    ASSERT_NOT_NULL(engine);
    
    // 检查统计信息
    ppdb_engine_stats_t stats;
    ppdb_engine_get_stats(engine, &stats);
    ASSERT_EQ(ppdb_base_counter_get(stats.total_txns), 0);
    ASSERT_EQ(ppdb_base_counter_get(stats.active_txns), 0);
    
    // 销毁
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
    return 0;
}

// 测试事务管理
int test_engine_transaction(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    ppdb_engine_txn_t* txn = NULL;
    
    // 初始化
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&base, &base_config));
    ASSERT_OK(ppdb_engine_init(&engine, base));
    
    // 开始事务
    ASSERT_OK(ppdb_engine_txn_begin(engine, &txn));
    ASSERT_NOT_NULL(txn);
    
    // 检查事务状态
    ppdb_engine_txn_stats_t txn_stats;
    ppdb_engine_txn_get_stats(txn, &txn_stats);
    ASSERT_TRUE(txn_stats.is_active);
    ASSERT_FALSE(txn_stats.is_committed);
    ASSERT_FALSE(txn_stats.is_rolledback);
    
    // 提交事务
    ASSERT_OK(ppdb_engine_txn_commit(txn));
    ppdb_engine_txn_get_stats(txn, &txn_stats);
    ASSERT_FALSE(txn_stats.is_active);
    ASSERT_TRUE(txn_stats.is_committed);
    
    // 检查引擎统计信息
    ppdb_engine_stats_t stats;
    ppdb_engine_get_stats(engine, &stats);
    ASSERT_EQ(ppdb_base_counter_get(stats.total_txns), 1);
    ASSERT_EQ(ppdb_base_counter_get(stats.active_txns), 0);
    
    // 清理
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
    return 0;
}

// 测试错误处理
int test_engine_errors(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    
    // 测试空指针
    ASSERT_ERR(ppdb_engine_init(NULL, base), PPDB_ENGINE_ERR_PARAM);
    ASSERT_ERR(ppdb_engine_init(&engine, NULL), PPDB_ENGINE_ERR_PARAM);
    
    // 测试未初始化的事务
    ASSERT_ERR(ppdb_engine_txn_commit(NULL), PPDB_ENGINE_ERR_PARAM);
    ASSERT_ERR(ppdb_engine_txn_rollback(NULL), PPDB_ENGINE_ERR_PARAM);
    
    return 0;
}

int main(void) {
    TEST_CASE(test_engine_init_destroy);
    TEST_CASE(test_engine_transaction);
    TEST_CASE(test_engine_errors);
    printf("\nTest summary:\n");
    printf("  Total: %d\n", g_test_count);
    printf("  Passed: %d\n", g_test_passed);
    printf("  Failed: %d\n", g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
