#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/database.h"
#include "../test_framework.h"

// 全局测试数据
static ppdb_base_t* g_base = NULL;
static ppdb_database_t* g_db = NULL;

// 测试初始化
static int test_setup(void) {
    printf("\n=== Setting up skiplist test environment ===\n");
    
    // 初始化 base 配置
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024 * 10,  // 10MB
        .thread_pool_size = 4,
        .thread_safe = true,
        .enable_logging = true,
        .log_level = PPDB_LOG_DEBUG
    };
    
    // 初始化 base 层
    ASSERT_OK(ppdb_base_init(&g_base, &base_config));

    // 初始化数据库配置
    ppdb_database_config_t db_config = {
        .base = g_base,
        .max_tables = 16,
        .max_txns = 1000,
        .enable_mvcc = true
    };

    // 初始化数据库层
    ASSERT_OK(ppdb_database_init(&g_db, &db_config));
    
    printf("Test environment setup completed\n");
    return 0;
}

// 测试清理
static int test_teardown(void) {
    printf("\n=== Cleaning up skiplist test environment ===\n");
    
    if (g_db) {
        ppdb_database_destroy(g_db);
        g_db = NULL;
    }

    if (g_base) {
        ppdb_base_destroy(g_base);
        g_base = NULL;
    }
    
    printf("Test environment cleanup completed\n");
    return 0;
}

// 创建测试用的key和value
static void create_test_kv(const char* key_str, const char* value_str, 
                          ppdb_key_t* key, ppdb_value_t* value) {
    key->size = strlen(key_str);
    key->data = ppdb_base_malloc(key->size);
    memcpy(key->data, key_str, key->size);
    
    value->size = strlen(value_str);
    value->data = ppdb_base_malloc(value->size);
    memcpy(value->data, value_str, value->size);
}

// 释放key和value
static void free_test_kv(ppdb_key_t* key, ppdb_value_t* value) {
    if (key && key->data) {
        ppdb_base_free(key->data);
        key->data = NULL;
    }
    if (value && value->data) {
        ppdb_base_free(value->data);
        value->data = NULL;
    }
}

// 基础功能测试
static int test_skiplist_basic(void) {
    printf("\n=== Running basic skiplist tests ===\n");
    
    // 创建事务
    ppdb_txn_t* txn = NULL;
    ASSERT_OK(ppdb_database_txn_begin(g_db, NULL, 0, &txn));

    // 创建表
    ppdb_database_table_t* table = NULL;
    ASSERT_OK(ppdb_database_table_create(g_db, txn, "test_table", &table));
    
    // 准备测试数据
    ppdb_key_t key1;
    ppdb_value_t value1;
    create_test_kv("key1", "value1", &key1, &value1);
    
    // 测试插入
    ASSERT_OK(ppdb_database_put(g_db, txn, "test_table", key1.data, key1.size, value1.data, value1.size));
    
    // 测试查找
    ppdb_value_t found_value;
    ASSERT_OK(ppdb_database_get(g_db, txn, "test_table", key1.data, key1.size, &found_value.data, &found_value.size));
    ASSERT_EQ(found_value.size, value1.size);
    ASSERT_EQ(memcmp(found_value.data, value1.data, value1.size), 0);
    
    // 测试删除
    ASSERT_OK(ppdb_database_delete(g_db, txn, "test_table", key1.data, key1.size));
    ASSERT_ERR(ppdb_database_get(g_db, txn, "test_table", key1.data, key1.size, &found_value.data, &found_value.size), PPDB_ERR_NOT_FOUND);
    
    // 清理
    free_test_kv(&key1, &value1);
    ppdb_database_txn_commit(txn);
    printf("Basic skiplist tests completed\n");
    return 0;
}

// 并发操作测试
typedef struct {
    ppdb_database_t* db;
    ppdb_database_table_t* table;
    int thread_id;
} thread_data_t;

static void* concurrent_insert_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    ppdb_database_t* db = data->db;
    int thread_id = data->thread_id;
    
    for (int i = 0; i < 100; i++) {
        char key_str[32], value_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d_%d", thread_id, i);
        snprintf(value_str, sizeof(value_str), "value_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, value_str, &key, &value);
        
        // 创建事务
        ppdb_txn_t* txn = NULL;
        ppdb_database_txn_begin(db, NULL, 0, &txn);
        
        // 插入数据
        ppdb_database_put(db, txn, "test_table", key.data, key.size, value.data, value.size);
        
        // 提交事务
        ppdb_database_txn_commit(txn);
        
        free_test_kv(&key, &value);
    }
    
    return NULL;
}

static int test_skiplist_concurrent(void) {
    printf("\n=== Running concurrent skiplist tests ===\n");
    
    // 创建表
    ppdb_txn_t* txn = NULL;
    ASSERT_OK(ppdb_database_txn_begin(g_db, NULL, 0, &txn));
    
    ppdb_database_table_t* table = NULL;
    ASSERT_OK(ppdb_database_table_create(g_db, txn, "test_table", &table));
    
    ASSERT_OK(ppdb_database_txn_commit(txn));
    
    // 创建多个线程进行并发插入
    ppdb_base_thread_t threads[4];
    thread_data_t thread_data[4];
    
    for (int i = 0; i < 4; i++) {
        thread_data[i].db = g_db;
        thread_data[i].table = table;
        thread_data[i].thread_id = i;
        ASSERT_OK(ppdb_base_thread_create(&threads[i], concurrent_insert_thread, &thread_data[i]));
    }
    
    // 等待所有线程完成
    for (int i = 0; i < 4; i++) {
        ASSERT_OK(ppdb_base_thread_join(threads[i], NULL));
    }
    
    // 验证结果
    ppdb_database_stats_t stats;
    ASSERT_OK(ppdb_database_get_stats(g_db, &stats));
    ASSERT_EQ(stats.total_records, 400);  // 4个线程 * 100个键值对
    
    printf("Concurrent skiplist tests completed\n");
    return 0;
}

// 边界条件测试
static int test_skiplist_boundary(void) {
    printf("\n=== Running boundary condition tests ===\n");
    
    ppdb_txn_t* txn = NULL;
    ASSERT_OK(ppdb_database_txn_begin(g_db, NULL, 0, &txn));
    
    ppdb_database_table_t* table = NULL;
    ASSERT_OK(ppdb_database_table_create(g_db, txn, "test_table", &table));
    
    // 测试NULL参数
    ppdb_key_t key;
    ppdb_value_t value;
    ASSERT_ERR(ppdb_database_put(g_db, txn, "test_table", NULL, 0, value.data, value.size), PPDB_ERR_PARAM);
    ASSERT_ERR(ppdb_database_put(g_db, txn, "test_table", key.data, key.size, NULL, 0), PPDB_ERR_PARAM);
    
    // 测试空键/值
    create_test_kv("", "value", &key, &value);
    ASSERT_ERR(ppdb_database_put(g_db, txn, "test_table", key.data, 0, value.data, value.size), PPDB_ERR_PARAM);
    free_test_kv(&key, &value);
    
    create_test_kv("key", "", &key, &value);
    ASSERT_ERR(ppdb_database_put(g_db, txn, "test_table", key.data, key.size, value.data, 0), PPDB_ERR_PARAM);
    free_test_kv(&key, &value);
    
    // 测试重复键
    create_test_kv("key", "value1", &key, &value);
    ASSERT_OK(ppdb_database_put(g_db, txn, "test_table", key.data, key.size, value.data, value.size));
    
    ppdb_value_t value2;
    create_test_kv("key", "value2", &key, &value2);
    ASSERT_OK(ppdb_database_put(g_db, txn, "test_table", key.data, key.size, value2.data, value2.size));  // 应该更新值
    
    ppdb_value_t found_value;
    ASSERT_OK(ppdb_database_get(g_db, txn, "test_table", key.data, key.size, &found_value.data, &found_value.size));
    ASSERT_EQ(found_value.size, value2.size);
    ASSERT_EQ(memcmp(found_value.data, value2.data, value2.size), 0);
    
    // 测试删除不存在的键
    create_test_kv("nonexistent", "", &key, &value);
    ASSERT_ERR(ppdb_database_delete(g_db, txn, "test_table", key.data, key.size), PPDB_ERR_NOT_FOUND);
    
    // 清理
    free_test_kv(&key, &value);
    free_test_kv(&key, &value2);
    ppdb_database_txn_commit(txn);
    printf("Boundary condition tests completed\n");
    return 0;
}

// 压力测试
static int test_skiplist_stress(void) {
    printf("\n=== Running stress tests ===\n");
    
    ppdb_txn_t* txn = NULL;
    ASSERT_OK(ppdb_database_txn_begin(g_db, NULL, 0, &txn));
    
    ppdb_database_table_t* table = NULL;
    ASSERT_OK(ppdb_database_table_create(g_db, txn, "test_table", &table));
    
    // 大量数据插入
    const int num_entries = 10000;
    printf("Inserting %d entries...\n", num_entries);
    
    for (int i = 0; i < num_entries; i++) {
        char key_str[32], value_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d", i);
        snprintf(value_str, sizeof(value_str), "value_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, value_str, &key, &value);
        
        ASSERT_OK(ppdb_database_put(g_db, txn, "test_table", key.data, key.size, value.data, value.size));
        
        free_test_kv(&key, &value);
        
        if (i % 1000 == 0) {
            printf("Inserted %d entries\n", i);
        }
    }
    
    // 验证所有数据
    printf("Verifying %d entries...\n", num_entries);
    for (int i = 0; i < num_entries; i++) {
        char key_str[32], value_str[32];
        snprintf(key_str, sizeof(key_str), "key_%d", i);
        snprintf(value_str, sizeof(value_str), "value_%d", i);
        
        ppdb_key_t key;
        ppdb_value_t value;
        create_test_kv(key_str, value_str, &key, &value);
        
        ppdb_value_t found_value;
        ASSERT_OK(ppdb_database_get(g_db, txn, "test_table", key.data, key.size, &found_value.data, &found_value.size));
        ASSERT_EQ(found_value.size, value.size);
        ASSERT_EQ(memcmp(found_value.data, value.data, value.size), 0);
        
        free_test_kv(&key, &value);
        
        if (i % 1000 == 0) {
            printf("Verified %d entries\n", i);
        }
    }
    
    ppdb_database_txn_commit(txn);
    printf("Stress tests completed\n");
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_setup);
    TEST_RUN(test_skiplist_basic);
    TEST_RUN(test_skiplist_concurrent);
    TEST_RUN(test_skiplist_boundary);
    TEST_RUN(test_skiplist_stress);
    TEST_RUN(test_teardown);
    
    TEST_REPORT();
    return 0;
}