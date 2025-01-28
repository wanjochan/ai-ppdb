#include "internal/poly/poly_memkv.h"
#include "internal/infra/infra_core.h"
#include "test/white/framework/test_framework.h"

// 基本操作测试
static void test_memkv_basic_ops(void) {
    // 初始化基础设施层
    infra_error_t init_err = infra_init();
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to initialize infra");

    // 创建并初始化 memkv 实例
    poly_memkv_t* store = NULL;
    poly_memkv_config_t config = {
        .max_key_size = 1024,
        .max_value_size = 4096,
        .engine_type = POLY_MEMKV_ENGINE_SQLITE,  // 默认使用 SQLite
        .path = ":memory:"
    };

    init_err = poly_memkv_create(&store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create memkv instance");
    TEST_ASSERT_NOT_NULL(store);

    // 配置存储引擎
    init_err = poly_memkv_configure(store, &config);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to configure memkv instance");

    // 打开存储
    init_err = poly_memkv_open(store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to open memkv store");

    // 测试 SET
    const char* key = "test_key";
    const char* value = "test_value";
    init_err = poly_memkv_set(store, key, strlen(key), value, strlen(value));
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to set key-value pair");

    // 测试 GET
    void* retrieved_value = NULL;
    size_t value_len = 0;
    init_err = poly_memkv_get(store, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to get value");
    TEST_ASSERT_EQUAL(strlen(value), value_len);
    TEST_ASSERT_MSG(memcmp(value, retrieved_value, value_len) == 0, "Value content mismatch");

    // 测试统计信息
    const poly_memkv_stats_t* stats = poly_memkv_get_stats(store);
    TEST_ASSERT_NOT_NULL(stats);
    TEST_ASSERT_EQUAL(1, poly_atomic_get(&stats->cmd_get));
    TEST_ASSERT_EQUAL(1, poly_atomic_get(&stats->cmd_set));
    TEST_ASSERT_EQUAL(1, poly_atomic_get(&stats->curr_items));
    TEST_ASSERT_EQUAL(1, poly_atomic_get(&stats->hits));

    // 测试 DEL
    init_err = poly_memkv_del(store, key, strlen(key));
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to delete key");

    // 验证删除
    init_err = poly_memkv_get(store, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_MSG(init_err == INFRA_ERROR_NOT_FOUND, "Key should not exist after deletion");

    // 再次检查统计信息
    stats = poly_memkv_get_stats(store);
    TEST_ASSERT_NOT_NULL(stats);
    TEST_ASSERT_EQUAL(2, poly_atomic_get(&stats->cmd_get));
    TEST_ASSERT_EQUAL(0, poly_atomic_get(&stats->curr_items));
    TEST_ASSERT_EQUAL(1, poly_atomic_get(&stats->hits));
    TEST_ASSERT_EQUAL(1, poly_atomic_get(&stats->misses));

    // 清理
    if (retrieved_value) infra_free(retrieved_value);
    poly_memkv_close(store);
    poly_memkv_destroy(store);
}

// 引擎切换测试
static void test_memkv_engine_switch(void) {
    // 初始化基础设施层
    infra_error_t init_err = infra_init();
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to initialize infra");

    // 创建并初始化 memkv 实例（SQLite）
    poly_memkv_t* store = NULL;
    poly_memkv_config_t config = {
        .max_key_size = 1024,
        .max_value_size = 4096,
        .engine_type = POLY_MEMKV_ENGINE_SQLITE,
        .path = ":memory:"
    };

    init_err = poly_memkv_create(&store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create memkv instance");

    // 配置存储引擎
    init_err = poly_memkv_configure(store, &config);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to configure memkv instance");

    // 打开存储
    init_err = poly_memkv_open(store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to open memkv store");

    // 插入测试数据
    const char* key = "switch_test_key";
    const char* value = "switch_test_value";
    init_err = poly_memkv_set(store, key, strlen(key), value, strlen(value));
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to set key-value pair in SQLite");

    // 切换到 DuckDB 引擎
    init_err = poly_memkv_switch_engine(store, POLY_MEMKV_ENGINE_DUCKDB, NULL);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to switch to DuckDB engine");

    // 验证数据是否正确迁移
    void* retrieved_value = NULL;
    size_t value_len = 0;
    init_err = poly_memkv_get(store, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to get value after engine switch");
    TEST_ASSERT_EQUAL(strlen(value), value_len);
    TEST_ASSERT_MSG(memcmp(value, retrieved_value, value_len) == 0, "Value content mismatch after engine switch");

    // 清理
    if (retrieved_value) infra_free(retrieved_value);
    poly_memkv_close(store);
    poly_memkv_destroy(store);
}

// 配置参数测试
static void test_memkv_config(void) {
    // 初始化基础设施层
    infra_error_t init_err = infra_init();
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to initialize infra");

    // 测试无效配置
    poly_memkv_t* store = NULL;
    poly_memkv_config_t invalid_config = {
        .max_key_size = 0,  // 无效的键大小
        .max_value_size = 4096,
        .engine_type = POLY_MEMKV_ENGINE_SQLITE,
        .path = ":memory:"
    };

    init_err = poly_memkv_create(&store);
    TEST_ASSERT_MSG(init_err == INFRA_ERROR_INVALID_PARAM, "Should fail with invalid key size");

    // 测试边界值
    poly_memkv_config_t valid_config = {
        .max_key_size = 1024,
        .max_value_size = 4096,
        .engine_type = POLY_MEMKV_ENGINE_SQLITE,
        .path = ":memory:"
    };

    init_err = poly_memkv_create(&store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create memkv instance with valid config");

    // 测试超出大小限制
    char* large_key = infra_malloc(valid_config.max_key_size + 1);
    memset(large_key, 'A', valid_config.max_key_size);
    large_key[valid_config.max_key_size] = '\0';

    init_err = poly_memkv_set(store, large_key, strlen(large_key), "value", 5);
    TEST_ASSERT_MSG(init_err == INFRA_ERROR_INVALID_PARAM, "Should fail with key size exceeding limit");

    // 清理
    infra_free(large_key);
    poly_memkv_destroy(store);
}

// 迭代器测试
static void test_memkv_iterator(void) {
    // 初始化基础设施层
    infra_error_t init_err = infra_init();
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to initialize infra");

    // 创建并初始化 memkv 实例
    poly_memkv_t* store = NULL;
    poly_memkv_config_t config = {
        .max_key_size = 1024,
        .max_value_size = 4096,
        .engine_type = POLY_MEMKV_ENGINE_SQLITE,
        .path = ":memory:"
    };

    init_err = poly_memkv_create(&store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create memkv instance");

    // 配置存储引擎
    init_err = poly_memkv_configure(store, &config);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to configure memkv instance");

    // 打开存储
    init_err = poly_memkv_open(store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to open memkv store");

    // 插入测试数据
    const char* keys[] = {"iter_key1", "iter_key2", "iter_key3"};
    const char* values[] = {"iter_value1", "iter_value2", "iter_value3"};
    for (int i = 0; i < 3; i++) {
        init_err = poly_memkv_set(store, keys[i], strlen(keys[i]), values[i], strlen(values[i]));
        TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to set test data");
    }

    // 创建迭代器
    poly_memkv_iter_t* iter = NULL;
    init_err = poly_memkv_iter_create(store, &iter);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create iterator");

    // 遍历数据
    int count = 0;
    char* key;
    void* value;
    size_t key_len, value_len;

    while ((init_err = poly_memkv_iter_next(iter, &key, &key_len, &value, &value_len)) == INFRA_OK) {
        TEST_ASSERT_NOT_NULL(key);
        TEST_ASSERT_NOT_NULL(value);
        count++;
        infra_free(key);
        infra_free(value);
    }

    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_MSG(init_err == INFRA_ERROR_NOT_FOUND, "Iterator should end with NOT_FOUND");

    // 清理
    poly_memkv_iter_destroy(iter);
    poly_memkv_close(store);
    poly_memkv_destroy(store);
}

int main(void) {
    TEST_BEGIN();
    
    // 运行所有测试
    RUN_TEST(test_memkv_basic_ops);
    RUN_TEST(test_memkv_engine_switch);
    RUN_TEST(test_memkv_config);
    RUN_TEST(test_memkv_iterator);
    
    TEST_END();
} 