#include "internal/poly/poly_duckdb.h"
#include "test/white/framework/test_framework.h"

// DuckDB 基本操作测试
static void test_duckdb_basic_ops(void) {
    void* db = NULL;
    infra_error_t err;
    
    // 初始化
    err = g_duckdb_interface.init(&db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to initialize DuckDB");
    
    // 打开数据库
    err = g_duckdb_interface.open(db, ":memory:");
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to open DuckDB database");
    TEST_ASSERT_NOT_NULL(db);
    
    // 测试 SET
    const char* key = "test_key";
    const char* value = "test_value";
    err = g_duckdb_interface.set(db, key, value, strlen(value));
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to set key-value pair");
    
    // 测试 GET
    void* retrieved_value = NULL;
    size_t value_len = 0;
    err = g_duckdb_interface.get(db, key, &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to get value");
    TEST_ASSERT_EQUAL(strlen(value), value_len);
    TEST_ASSERT_MSG(memcmp(value, retrieved_value, value_len) == 0, "Value content mismatch");
    
    // 测试 DEL
    err = g_duckdb_interface.del(db, key);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to delete key");
    
    // 验证删除
    err = g_duckdb_interface.get(db, key, &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_ERROR_NOT_FOUND, "Key should not exist after deletion");
    
    // 清理
    if (retrieved_value) infra_free(retrieved_value);
    g_duckdb_interface.cleanup(db);
}

// DuckDB 迭代器测试
static void test_duckdb_iterator(void) {
    void* db = NULL;
    void* iter = NULL;
    infra_error_t err;
    
    // 初始化
    err = g_duckdb_interface.init(&db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to initialize DuckDB");
    
    // 打开数据库
    err = g_duckdb_interface.open(db, ":memory:");
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to open DuckDB database");
    
    // 插入测试数据
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    for (int i = 0; i < 3; i++) {
        err = g_duckdb_interface.set(db, keys[i], values[i], strlen(values[i]));
        TEST_ASSERT_MSG(err == INFRA_OK, "Failed to set test data");
    }
    
    // 创建迭代器
    err = g_duckdb_interface.iter_create(db, &iter);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to create iterator");
    
    // 遍历数据
    int count = 0;
    char* key;
    void* value;
    size_t value_len;
    
    while ((err = g_duckdb_interface.iter_next(iter, &key, &value, &value_len)) == INFRA_OK) {
        TEST_ASSERT_NOT_NULL(key);
        TEST_ASSERT_NOT_NULL(value);
        count++;
        infra_free(key);
        infra_free(value);
    }
    
    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_MSG(err == INFRA_ERROR_NOT_FOUND, "Iterator should end with NOT_FOUND");
    
    // 清理
    g_duckdb_interface.iter_destroy(iter);
    g_duckdb_interface.cleanup(db);
}

// DuckDB 事务测试
static void test_duckdb_transaction(void) {
    void* db = NULL;
    infra_error_t err;
    
    // 初始化
    err = g_duckdb_interface.init(&db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to initialize DuckDB");
    
    // 打开数据库
    err = g_duckdb_interface.open(db, ":memory:");
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to open DuckDB database");
    
    // 开始事务
    err = g_duckdb_interface.exec(db, "BEGIN TRANSACTION");
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to begin transaction");
    
    // 插入数据
    const char* key = "tx_key";
    const char* value = "tx_value";
    err = g_duckdb_interface.set(db, key, value, strlen(value));
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to set in transaction");
    
    // 提交事务
    err = g_duckdb_interface.exec(db, "COMMIT");
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to commit transaction");
    
    // 验证数据
    void* retrieved_value = NULL;
    size_t value_len = 0;
    err = g_duckdb_interface.get(db, key, &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to get committed value");
    
    // 清理
    if (retrieved_value) infra_free(retrieved_value);
    g_duckdb_interface.cleanup(db);
}

int main(void) {
    TEST_BEGIN();
    
    // 运行所有测试
    RUN_TEST(test_duckdb_basic_ops);
    RUN_TEST(test_duckdb_iterator);
    RUN_TEST(test_duckdb_transaction);
    
    TEST_END();
} 