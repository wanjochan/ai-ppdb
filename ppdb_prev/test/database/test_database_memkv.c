#include <cosmopolitan.h>
#include "internal/database.h"
#include "../test_framework.h"

// 全局测试数据
static ppdb_database_t* g_database = NULL;
static ppdb_database_config_t g_config;

// 测试初始化
static int test_setup(void) {
    // 初始化配置
    g_config = (ppdb_database_config_t){
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .cache_size = 1024 * 1024,         // 1MB
        .enable_mvcc = true,
        .enable_logging = true,
        .sync_on_commit = false,
        .default_isolation = PPDB_TXN_READ_COMMITTED,
        .lock_timeout_ms = 1000,
        .txn_timeout_ms = 5000
    };

    // 创建数据库实例
    TEST_ASSERT_EQUALS(ppdb_database_create(&g_database, &g_config), PPDB_OK);
    TEST_ASSERT_NOT_NULL(g_database);

    return 0;
}

// 测试清理
static int test_teardown(void) {
    if (g_database) {
        ppdb_database_destroy(g_database);
        g_database = NULL;
    }
    return 0;
}

// 创建测试键值对
static void create_test_kv(const char* key_str, const char* value_str, 
                          ppdb_key_t* key, ppdb_value_t* value) {
    key->data = (void*)key_str;
    key->size = strlen(key_str);
    value->data = (void*)value_str;
    value->size = strlen(value_str);
}

// 基本功能测试
static int test_memkv_basic(void) {
    ppdb_key_t key;
    ppdb_value_t value, get_value = {0};
    ppdb_database_txn_t* txn = NULL;

    // 开始事务
    TEST_ASSERT_EQUALS(ppdb_database_txn_begin(g_database, &txn, 0), PPDB_OK);
    TEST_ASSERT_NOT_NULL(txn);

    // 插入测试
    create_test_kv("test_key", "test_value", &key, &value);
    TEST_ASSERT_EQUALS(ppdb_database_put(txn, &key, &value), PPDB_OK);

    // 查询测试
    TEST_ASSERT_EQUALS(ppdb_database_get(txn, &key, &get_value), PPDB_OK);
    TEST_ASSERT_EQUALS(get_value.size, value.size);
    TEST_ASSERT_EQUALS(memcmp(get_value.data, value.data, value.size), 0);

    // 删除测试
    TEST_ASSERT_EQUALS(ppdb_database_delete(txn, &key), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_database_get(txn, &key, &get_value), PPDB_DATABASE_ERR_NOT_FOUND);

    // 提交事务
    TEST_ASSERT_EQUALS(ppdb_database_txn_commit(txn), PPDB_OK);

    return 0;
}

// 测试用例数组
static test_case_t test_cases[] = {
    {
        .name = "Basic MemKV Operations",
        .description = "Tests basic operations (put/get/delete) with transactions",
        .fn = test_memkv_basic,
        .timeout_seconds = 10,
        .skip = false
    }
};

// 测试套件
static test_suite_t memkv_test_suite = {
    .name = "MemKV Database Test Suite",
    .setup = test_setup,
    .teardown = test_teardown,
    .cases = test_cases,
    .num_cases = sizeof(test_cases) / sizeof(test_cases[0])
};

int main(void) {
    TEST_INIT();
    
    printf("\n=== PPDB MemKV Database Test Suite ===\n");
    printf("Starting tests...\n\n");

    int result = run_test_suite(&memkv_test_suite);
    
    TEST_REPORT();
    return result;
}