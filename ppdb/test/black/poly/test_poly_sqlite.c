#include "internal/poly/poly_sqlite.h"
#include "test/white/framework/test_framework.h"

// SQLite 基本操作测试
static void test_sqlite_basic_ops(void) {
    poly_sqlite_db_t* db = NULL;
    infra_error_t err;
    
    // 打开数据库
    err = poly_sqlite_open(":memory:", &db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to open SQLite database");
    TEST_ASSERT_NOT_NULL(db);
    
    // 测试 PUT
    const char* key = "test_key";
    const char* value = "test_value";
    err = poly_sqlite_put(db, key, strlen(key), value, strlen(value));
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to put key-value pair");
    
    // 测试 GET
    void* retrieved_value = NULL;
    size_t value_len = 0;
    err = poly_sqlite_get(db, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to get value");
    TEST_ASSERT_EQUAL(strlen(value), value_len);
    TEST_ASSERT_MSG(memcmp(value, retrieved_value, value_len) == 0, "Value content mismatch");
    
    // 测试 DEL
    err = poly_sqlite_del(db, key, strlen(key));
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to delete key");
    
    // 验证删除
    err = poly_sqlite_get(db, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_ERROR_NOT_FOUND, "Key should not exist after deletion");
    
    // 清理
    if (retrieved_value) infra_free(retrieved_value);
    poly_sqlite_close(db);
}

// SQLite 迭代器测试
static void test_sqlite_iterator(void) {
    poly_sqlite_db_t* db = NULL;
    poly_sqlite_iter_t* iter = NULL;
    infra_error_t err;
    
    // 打开数据库
    err = poly_sqlite_open(":memory:", &db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to open SQLite database");
    
    // 插入测试数据
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    for (int i = 0; i < 3; i++) {
        err = poly_sqlite_put(db, keys[i], strlen(keys[i]), values[i], strlen(values[i]));
        TEST_ASSERT_MSG(err == INFRA_OK, "Failed to put test data");
    }
    
    // 创建迭代器
    err = poly_sqlite_iter_create(db, &iter);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to create iterator");
    
    // 遍历数据
    int count = 0;
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;
    
    while ((err = poly_sqlite_iter_next(iter, &key, &key_len, &value, &value_len)) == INFRA_OK) {
        TEST_ASSERT_NOT_NULL(key);
        TEST_ASSERT_NOT_NULL(value);
        count++;
        infra_free(key);
        infra_free(value);
    }
    
    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_MSG(err == INFRA_ERROR_NOT_FOUND, "Iterator should end with NOT_FOUND");
    
    // 清理
    poly_sqlite_iter_destroy(iter);
    poly_sqlite_close(db);
}

// SQLite 事务测试
static void test_sqlite_transaction(void) {
    poly_sqlite_db_t* db = NULL;
    infra_error_t err;
    
    // 打开数据库
    err = poly_sqlite_open(":memory:", &db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to open SQLite database");
    
    // 开始事务
    err = poly_sqlite_begin(db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to begin transaction");
    
    // 插入数据
    const char* key = "tx_key";
    const char* value = "tx_value";
    err = poly_sqlite_put(db, key, strlen(key), value, strlen(value));
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to put in transaction");
    
    // 提交事务
    err = poly_sqlite_commit(db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to commit transaction");
    
    // 验证数据
    void* retrieved_value = NULL;
    size_t value_len = 0;
    err = poly_sqlite_get(db, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to get committed value");
    
    // 清理
    if (retrieved_value) infra_free(retrieved_value);
    poly_sqlite_close(db);
}

int main(void) {
    TEST_BEGIN();
    
    // 运行所有测试
    RUN_TEST(test_sqlite_basic_ops);
    RUN_TEST(test_sqlite_iterator);
    RUN_TEST(test_sqlite_transaction);
    
    TEST_END();
} 