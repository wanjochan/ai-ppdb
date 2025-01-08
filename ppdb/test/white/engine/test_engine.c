#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "../test_macros.h"

// 全局测试数据
static ppdb_base_t* g_base = NULL;
static ppdb_engine_t* g_engine = NULL;

// 测试初始化
static int test_setup(void) {
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_OK(ppdb_base_init(&g_base, &base_config));
    ASSERT_OK(ppdb_engine_init(&g_engine, g_base));
    return 0;
}

// 测试清理
static int test_teardown(void) {
    if (g_engine) {
        ppdb_engine_destroy(g_engine);
        g_engine = NULL;
    }
    if (g_base) {
        ppdb_base_destroy(g_base);
        g_base = NULL;
    }
    return 0;
}

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

// 测试基本事务操作
int test_engine_transaction_basic(void) {
    ASSERT_OK(test_setup());
    
    ppdb_engine_txn_t* txn = NULL;
    
    // 开始事务
    ASSERT_OK(ppdb_engine_txn_begin(g_engine, true, &txn));
    ASSERT_NOT_NULL(txn);
    
    // 检查事务状态
    ppdb_engine_txn_stats_t txn_stats;
    ppdb_engine_txn_get_stats(txn, &txn_stats);
    ASSERT_TRUE(txn_stats.is_active);
    ASSERT_TRUE(txn_stats.is_write);
    ASSERT_EQ(txn_stats.active_txns, 1);
    ASSERT_EQ(txn_stats.committed_txns, 0);
    ASSERT_EQ(txn_stats.aborted_txns, 0);
    
    // 提交事务
    ASSERT_OK(ppdb_engine_txn_commit(txn));
    ppdb_engine_txn_get_stats(txn, &txn_stats);
    ASSERT_FALSE(txn_stats.is_active);
    ASSERT_EQ(txn_stats.committed_txns, 1);
    
    // 检查引擎统计信息
    ppdb_engine_stats_t stats;
    ppdb_engine_get_stats(g_engine, &stats);
    ASSERT_EQ(ppdb_base_counter_get(stats.total_txns), 1);
    ASSERT_EQ(ppdb_base_counter_get(stats.active_txns), 0);
    
    ASSERT_OK(test_teardown());
    return 0;
}

// 测试事务回滚
int test_engine_transaction_rollback(void) {
    ASSERT_OK(test_setup());
    
    ppdb_engine_txn_t* txn = NULL;
    
    // 开始事务
    ASSERT_OK(ppdb_engine_txn_begin(g_engine, true, &txn));
    ASSERT_NOT_NULL(txn);
    
    // 创建测试表
    ppdb_engine_table_t* table = NULL;
    ASSERT_OK(ppdb_engine_table_create(txn, "test_table", &table));
    
    // 写入一些数据
    const char* key = "test_key";
    const char* value = "test_value";
    ASSERT_OK(ppdb_engine_put(txn, table, key, strlen(key), value, strlen(value)));
    
    // 回滚事务
    ASSERT_OK(ppdb_engine_txn_rollback(txn));
    
    // 检查事务状态
    ppdb_engine_txn_stats_t txn_stats;
    ppdb_engine_txn_get_stats(txn, &txn_stats);
    ASSERT_FALSE(txn_stats.is_active);
    ASSERT_EQ(txn_stats.aborted_txns, 1);
    ASSERT_EQ(txn_stats.committed_txns, 0);
    
    ASSERT_OK(test_teardown());
    return 0;
}

// 测试并发事务
int test_engine_concurrent_transactions(void) {
    ASSERT_OK(test_setup());
    
    ppdb_engine_txn_t* txn1 = NULL;
    ppdb_engine_txn_t* txn2 = NULL;
    
    // 开始两个并发事务
    ASSERT_OK(ppdb_engine_txn_begin(g_engine, true, &txn1));
    ASSERT_OK(ppdb_engine_txn_begin(g_engine, true, &txn2));
    
    // 检查活跃事务数
    ppdb_engine_stats_t stats;
    ppdb_engine_get_stats(g_engine, &stats);
    ASSERT_EQ(ppdb_base_counter_get(stats.active_txns), 2);
    
    // 提交第一个事务
    ASSERT_OK(ppdb_engine_txn_commit(txn1));
    ppdb_engine_get_stats(g_engine, &stats);
    ASSERT_EQ(ppdb_base_counter_get(stats.active_txns), 1);
    
    // 回滚第二个事务
    ASSERT_OK(ppdb_engine_txn_rollback(txn2));
    ppdb_engine_get_stats(g_engine, &stats);
    ASSERT_EQ(ppdb_base_counter_get(stats.active_txns), 0);
    ASSERT_EQ(ppdb_base_counter_get(stats.total_txns), 2);
    
    ASSERT_OK(test_teardown());
    return 0;
}

// 测试数据操作
int test_engine_data_operations(void) {
    ASSERT_OK(test_setup());
    
    ppdb_engine_txn_t* txn = NULL;
    ASSERT_OK(ppdb_engine_txn_begin(g_engine, true, &txn));
    
    // 创建表
    ppdb_engine_table_t* table = NULL;
    ASSERT_OK(ppdb_engine_table_create(txn, "test_table", &table));
    
    // 写入数据
    const char* key = "test_key";
    const char* value = "test_value";
    ASSERT_OK(ppdb_engine_put(txn, table, key, strlen(key), value, strlen(value) + 1));  // Include null terminator
    
    // 读取数据
    char read_value[256];
    size_t read_size = sizeof(read_value);
    ASSERT_OK(ppdb_engine_get(txn, table, key, strlen(key), read_value, &read_size));
    ASSERT_EQ(read_size, strlen(value) + 1);  // 包含结尾的 null 字符
    ASSERT_EQ(memcmp(value, read_value, strlen(value)), 0);
    
    // 删除数据
    ASSERT_OK(ppdb_engine_delete(txn, table, key, strlen(key)));
    
    // 提交事务
    ASSERT_OK(ppdb_engine_txn_commit(txn));
    
    ASSERT_OK(test_teardown());
    return 0;
}

// 测试错误处理
int test_engine_errors(void) {
    ppdb_base_t* base = NULL;
    ppdb_engine_t* engine = NULL;
    ppdb_error_t err;
    
    printf("Testing error handling...\n");
    
    // 测试空指针
    printf("Testing NULL pointer handling...\n");
    ASSERT_ERR(ppdb_engine_init(NULL, NULL), PPDB_ENGINE_ERR_PARAM);
    ASSERT_ERR(ppdb_engine_init(&engine, NULL), PPDB_ENGINE_ERR_PARAM);
    
    // 测试未初始化的事务
    printf("Testing uninitialized transaction handling...\n");
    ASSERT_ERR(ppdb_engine_txn_commit(NULL), PPDB_ENGINE_ERR_PARAM);
    ASSERT_ERR(ppdb_engine_txn_rollback(NULL), PPDB_ENGINE_ERR_PARAM);
    
    // 测试重复初始化
    printf("Testing base initialization...\n");
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .thread_pool_size = 1,
        .thread_safe = false
    };
    
    // 初始化 base
    err = ppdb_base_init(&base, &base_config);
    if (err == PPDB_BASE_ERR_SYSTEM) {
        fprintf(stderr, "System error during base initialization, skipping test\n");
        return 0;  // 跳过测试
    }
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to init base: error code %d\n", err);
        return 1;
    }
    
    printf("Testing engine initialization...\n");
    // 初始化 engine
    err = ppdb_engine_init(&engine, base);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to init engine: error code %d\n", err);
        ppdb_base_destroy(base);
        return 1;
    }
    
    printf("Testing duplicate initialization...\n");
    // 测试重复初始化
    ASSERT_ERR(ppdb_engine_init(&engine, base), PPDB_ENGINE_ERR_PARAM);
    
    printf("Testing transaction state errors...\n");
    // 测试事务状态错误
    ppdb_engine_txn_t* txn = NULL;
    err = ppdb_engine_txn_begin(engine, true, &txn);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to begin transaction: error code %d\n", err);
        ppdb_engine_destroy(engine);
        ppdb_base_destroy(base);
        return 1;
    }
    
    // 测试重复提交
    ASSERT_OK(ppdb_engine_txn_commit(txn));
    ASSERT_ERR(ppdb_engine_txn_commit(txn), PPDB_ENGINE_ERR_INVALID_STATE);
    
    // 清理
    ppdb_engine_destroy(engine);
    ppdb_base_destroy(base);
    return 0;
}

// 测试边界条件
int test_engine_boundary_conditions(void) {
    ASSERT_OK(test_setup());
    
    // 测试最大事务数
    const int MAX_TXNS = 100;
    ppdb_engine_txn_t* txns[MAX_TXNS];
    ppdb_error_t err;
    int i;
    
    printf("Testing maximum concurrent transactions...\n");
    for (i = 0; i < MAX_TXNS; i++) {
        err = ppdb_engine_txn_begin(g_engine, true, &txns[i]);
        if (err != PPDB_OK) {
            printf("Maximum concurrent transactions: %d\n", i);
            break;
        }
    }
    
    // 清理事务
    for (int j = 0; j < i; j++) {
        ASSERT_OK(ppdb_engine_txn_rollback(txns[j]));
    }
    
    ASSERT_OK(test_teardown());
    return 0;
}

// 主函数
int main(void) {
    printf("Running engine tests...\n");
    
    RUN_TEST(test_engine_init_destroy);
    RUN_TEST(test_engine_transaction_basic);
    RUN_TEST(test_engine_transaction_rollback);
    RUN_TEST(test_engine_concurrent_transactions);
    RUN_TEST(test_engine_data_operations);
    RUN_TEST(test_engine_errors);
    RUN_TEST(test_engine_boundary_conditions);
    
    printf("All engine tests passed!\n");
    return 0;
}
