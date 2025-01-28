#include "internal/poly/poly_sqlite.h"
#include "test/white/framework/test_framework.h"
#include "internal/infra/infra_core.h"

// 基本操作测试
static void test_sqlite_basic_ops(void) {
    poly_sqlite_db_t* db = NULL;
    infra_error_t err;
    
    // 打开数据库
    err = poly_sqlite_open(&db, ":memory:");
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to open database");
    TEST_ASSERT_NOT_NULL(db);
    
    // 测试 SET
    const char* key = "test_key";
    const char* value = "test_value";
    err = poly_sqlite_set(db, key, strlen(key), value, strlen(value));
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to set key-value pair");
    
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

// 测试迭代器功能
static void test_sqlite_iterator() {
    poly_sqlite_db_t* db = NULL;
    infra_error_t err = poly_sqlite_open(&db, ":memory:");
    if (err != INFRA_OK) {
        return;
    }

    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    int count = sizeof(keys) / sizeof(keys[0]);

    // 插入测试数据
    for (int i = 0; i < count; i++) {
        err = poly_sqlite_set(db, keys[i], strlen(keys[i]), values[i], strlen(values[i]));
        if (err != INFRA_OK) {
            poly_sqlite_close(db);
            return;
        }
    }

    // 创建迭代器
    poly_sqlite_iter_t* iter = NULL;
    err = poly_sqlite_iter_create(db, &iter);
    if (err != INFRA_OK) {
        poly_sqlite_close(db);
        return;
    }

    // 遍历所有键值对
    char* key = NULL;
    size_t key_len = 0;
    void* value = NULL;
    size_t value_len = 0;

    while ((err = poly_sqlite_iter_next(iter, &key, &key_len, &value, &value_len)) == INFRA_OK) {
        // 验证数据
        if (key && value) {
            // 这里可以添加具体的验证逻辑
        }
    }

    // 清理资源
    poly_sqlite_iter_destroy(iter);
    poly_sqlite_close(db);
}

// 测试事务功能
static infra_error_t test_sqlite_transaction(void) {
    poly_sqlite_db_t* db = NULL;
    infra_error_t err = poly_sqlite_open(&db, ":memory:");
    if (err != INFRA_OK) {
        return err;
    }

    poly_sqlite_ctx_t* ctx = (poly_sqlite_ctx_t*)db;
    err = poly_sqlite_exec(ctx, "BEGIN TRANSACTION");
    if (err != INFRA_OK) {
        poly_sqlite_close(db);
        return err;
    }

    // 执行一些操作
    const char* key = "tx_key";
    const char* value = "tx_value";
    err = poly_sqlite_set(db, key, strlen(key), value, strlen(value));
    if (err != INFRA_OK) {
        poly_sqlite_exec(ctx, "ROLLBACK");
        poly_sqlite_close(db);
        return err;
    }

    if (err != INFRA_OK) {
        poly_sqlite_exec(ctx, "ROLLBACK");
        poly_sqlite_close(db);
        return err;
    }

    err = poly_sqlite_exec(ctx, "COMMIT");
    if (err != INFRA_OK) {
        poly_sqlite_exec(ctx, "ROLLBACK");
        poly_sqlite_close(db);
        return err;
    }

    poly_sqlite_close(db);
    return INFRA_OK;
}

int main(void) {
    TEST_BEGIN();
    
    // 运行所有测试
    RUN_TEST(test_sqlite_basic_ops);
    RUN_TEST(test_sqlite_iterator);
    RUN_TEST(test_sqlite_transaction);
    
    TEST_END();
} 