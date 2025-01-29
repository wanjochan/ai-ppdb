#include "internal/poly/poly_memkv.h"
#include "internal/infra/infra_core.h"
#include "test/white/framework/test_framework.h"

// 基本操作测试
static void test_memkv_basic_ops(void) {
    // 初始化基础设施层
    infra_error_t err = infra_init();
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to initialize infra");

    // 创建配置
    poly_memkv_config_t config = {
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 4096,
        .memory_limit = 1024 * 1024,  // 1MB memory limit
        .enable_compression = false,
        .allow_fallback = true,
        .read_only = false
    };

    // 创建并打开数据库
    poly_memkv_db_t* db = NULL;
    err = poly_memkv_create(&config, &db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to create memkv database");
    TEST_ASSERT_NOT_NULL(db);

    // 测试 SET
    const char* key = "test_key";
    const char* value = "test_value";
    err = poly_memkv_set(db, key, value, strlen(value) + 1);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to set key-value pair");

    // 测试 GET
    void* retrieved_value = NULL;
    size_t value_len = 0;
    err = poly_memkv_get(db, key, &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to get value");
    TEST_ASSERT_EQUAL(strlen(value) + 1, value_len);
    TEST_ASSERT_MSG(memcmp(value, retrieved_value, value_len) == 0, "Value content mismatch");

    // 测试 DEL
    err = poly_memkv_del(db, key);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to delete key");

    // 验证删除
    err = poly_memkv_get(db, key, &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_ERROR_NOT_FOUND, "Key should not exist after deletion");

    // 测试迭代器
    const char* test_keys[] = {"key1", "key2", "key3"};
    const char* test_values[] = {"value1", "value2", "value3"};
    for (int i = 0; i < 3; i++) {
        err = poly_memkv_set(db, test_keys[i], test_values[i], strlen(test_values[i]) + 1);
        TEST_ASSERT_MSG(err == INFRA_OK, "Failed to set test key-value pair");
    }

    poly_memkv_iter_t* iter = NULL;
    err = poly_memkv_iter_create(db, &iter);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to create iterator");

    int count = 0;
    char* iter_key = NULL;
    void* iter_value = NULL;
    size_t iter_value_len = 0;
    while (poly_memkv_iter_next(iter, &iter_key, &iter_value, &iter_value_len) == INFRA_OK) {
        TEST_ASSERT_MSG(count < 3, "Iterator returned too many items");
        TEST_ASSERT_MSG(strcmp(iter_key, test_keys[count]) == 0, "Iterator key mismatch");
        TEST_ASSERT_MSG(strcmp(iter_value, test_values[count]) == 0, "Iterator value mismatch");
        infra_free(iter_key);
        infra_free(iter_value);
        count++;
    }
    TEST_ASSERT_EQUAL(3, count);

    // 清理
    poly_memkv_iter_destroy(iter);
    if (retrieved_value) infra_free(retrieved_value);
    poly_memkv_destroy(db);
}

// 引擎切换测试
static void test_memkv_engine_switch(void) {
    // 初始化基础设施层
    infra_error_t err = infra_init();
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to initialize infra");

    // 创建 SQLite 配置
    poly_memkv_config_t sqlite_config = {
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 4096,
        .memory_limit = 1024 * 1024,
        .enable_compression = false,
        .allow_fallback = true,
        .read_only = false
    };

    // 创建数据库
    poly_memkv_db_t* db = NULL;
    err = poly_memkv_create(&sqlite_config, &db);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to create SQLite database");

    // 写入测试数据
    const char* key = "test_key";
    const char* value = "test_value";
    err = poly_memkv_set(db, key, value, strlen(value) + 1);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to set key-value pair in SQLite");

    // 切换到 DuckDB
    poly_memkv_config_t duckdb_config = {
        .engine = POLY_MEMKV_ENGINE_DUCKDB,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 4096,
        .memory_limit = 1024 * 1024,
        .enable_compression = false,
        .allow_fallback = true,
        .read_only = false
    };

    err = poly_memkv_switch_engine(db, POLY_MEMKV_ENGINE_DUCKDB, &duckdb_config);
    if (err == INFRA_ERROR_NOT_SUPPORTED) {
        printf("DuckDB engine not supported, skipping engine switch test\n");
        poly_memkv_destroy(db);
        return;
    }
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to switch to DuckDB engine");

    // 验证数据迁移
    void* retrieved_value = NULL;
    size_t value_len = 0;
    err = poly_memkv_get(db, key, &retrieved_value, &value_len);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to get value after engine switch");
    TEST_ASSERT_EQUAL(strlen(value) + 1, value_len);
    TEST_ASSERT_MSG(memcmp(value, retrieved_value, value_len) == 0, "Value content mismatch after engine switch");

    // 清理
    if (retrieved_value) infra_free(retrieved_value);
    poly_memkv_destroy(db);
}

int main(void) {
    TEST_BEGIN("Memory KV Store Tests");
    
    RUN_TEST(test_memkv_basic_ops);
    RUN_TEST(test_memkv_engine_switch);
    
    TEST_END();
    return 0;
} 