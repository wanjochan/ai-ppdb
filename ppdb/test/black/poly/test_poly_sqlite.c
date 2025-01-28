#include "internal/poly/poly_sqlite.h"
#include "internal/infra/infra_core.h"
#include "test/white/framework/test_framework.h"

// 基本操作测试
static void test_sqlite_basic_ops(void) {
    void* db;
    infra_error_t err;
    const char* key = "test_key";
    const char* value = "test_value";
    void* retrieved_value;
    size_t value_len;

    // 初始化
    err = g_sqlite_interface.init(&db);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 打开数据库
    err = g_sqlite_interface.open(db, ":memory:");
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 设置键值对
    err = g_sqlite_interface.set(db, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 获取键值对
    err = g_sqlite_interface.get(db, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_EQUAL(err, INFRA_OK);
    TEST_ASSERT_EQUAL(value_len, strlen(value) + 1);
    TEST_ASSERT_EQUAL_STR(value, (char*)retrieved_value);
    infra_free(retrieved_value);

    // 删除键值对
    err = g_sqlite_interface.del(db, key, strlen(key));
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 验证键值对已被删除
    err = g_sqlite_interface.get(db, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_EQUAL(err, INFRA_ERROR_NOT_FOUND);

    // 清理
    g_sqlite_interface.cleanup(db);
}

// 迭代器测试
static void test_sqlite_iterator(void) {
    void* db;
    void* iter;
    infra_error_t err;
    char* key;
    void* value;
    size_t value_len;
    int count = 0;

    // 初始化
    err = g_sqlite_interface.init(&db);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 打开数据库
    err = g_sqlite_interface.open(db, ":memory:");
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 插入测试数据
    err = g_sqlite_interface.set(db, "key1", strlen("key1"), "value1", strlen("value1") + 1);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    err = g_sqlite_interface.set(db, "key2", strlen("key2"), "value2", strlen("value2") + 1);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    err = g_sqlite_interface.set(db, "key3", strlen("key3"), "value3", strlen("value3") + 1);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 创建迭代器
    err = g_sqlite_interface.iter_create(db, &iter);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 遍历所有键值对
    while (g_sqlite_interface.iter_next(iter, &key, &value, &value_len) == INFRA_OK) {
        TEST_ASSERT_NOT_NULL(key);
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_TRUE(value_len > 0);

        // 验证键值对格式
        TEST_ASSERT_EQUAL(0, strncmp(key, "key", 3));
        TEST_ASSERT_EQUAL(0, strncmp(value, "value", 5));
        TEST_ASSERT_TRUE(key[3] >= '1' && key[3] <= '3');
        TEST_ASSERT_EQUAL(key[3], ((char*)value)[5]);

        infra_free(key);
        infra_free(value);
        count++;
    }

    // 验证遍历到的键值对数量
    TEST_ASSERT_EQUAL(3, count);

    // 销毁迭代器
    g_sqlite_interface.iter_destroy(iter);

    // 清理
    g_sqlite_interface.cleanup(db);
}

// 事务测试
static void test_sqlite_transaction(void) {
    void* db;
    infra_error_t err;
    const char* key = "test_key";
    const char* value = "test_value";
    void* retrieved_value;
    size_t value_len;

    // 初始化
    err = g_sqlite_interface.init(&db);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 打开数据库
    err = g_sqlite_interface.open(db, ":memory:");
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 设置键值对
    err = g_sqlite_interface.set(db, key, strlen(key), value, strlen(value) + 1);
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 执行事务
    err = g_sqlite_interface.exec(db, "BEGIN TRANSACTION;");
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 在事务中修改数据
    err = g_sqlite_interface.exec(db, "UPDATE kv_store SET value = X'6E657776616C7565' WHERE key = 'test_key';");
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 提交事务
    err = g_sqlite_interface.exec(db, "COMMIT;");
    TEST_ASSERT_EQUAL(err, INFRA_OK);

    // 验证修改后的值
    err = g_sqlite_interface.get(db, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_EQUAL(err, INFRA_OK);
    TEST_ASSERT_EQUAL_STR("newvalue", (char*)retrieved_value);
    infra_free(retrieved_value);

    // 清理
    g_sqlite_interface.cleanup(db);
}

int main(void) {
    TEST_BEGIN();
    
    // 运行所有测试
    RUN_TEST(test_sqlite_basic_ops);
    RUN_TEST(test_sqlite_iterator);
    RUN_TEST(test_sqlite_transaction);
    
    TEST_END();
} 