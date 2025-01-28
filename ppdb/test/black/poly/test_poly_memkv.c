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
    init_err = poly_memkv_create(&store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create memkv instance");
    TEST_ASSERT_NOT_NULL(store);

    // 配置存储引擎
    poly_memkv_config_t config = {
        .engine = POLY_MEMKV_ENGINE_SQLITE,  // 默认使用 SQLite
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 4096,
        .memory_limit = 1024 * 1024,  // 1MB memory limit
        .enable_compression = false
    };

    init_err = poly_memkv_configure(store, &config);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to configure memkv instance");

    // 打开存储
    init_err = poly_memkv_open(store);
    if (init_err != INFRA_OK) {
        poly_memkv_destroy(store);
        TEST_FAIL_MSG("Failed to open memkv store");
    }

    // 测试 SET
    const char* key = "test_key";
    const char* value = "test_value";
    init_err = poly_memkv_set(store, key, strlen(key), value, strlen(value) + 1);
    if (init_err != INFRA_OK) {
        poly_memkv_close(store);
        poly_memkv_destroy(store);
        TEST_FAIL_MSG("Failed to set key-value pair");
    }

    // 测试 GET
    void* retrieved_value = NULL;
    size_t value_len = 0;
    init_err = poly_memkv_get(store, key, strlen(key), &retrieved_value, &value_len);
    if (init_err != INFRA_OK) {
        poly_memkv_close(store);
        poly_memkv_destroy(store);
        TEST_FAIL_MSG("Failed to get value");
    }
    TEST_ASSERT_EQUAL(strlen(value) + 1, value_len);
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
    if (init_err != INFRA_OK) {
        infra_free(retrieved_value);
        poly_memkv_close(store);
        poly_memkv_destroy(store);
        TEST_FAIL_MSG("Failed to delete key");
    }

    // 验证删除
    init_err = poly_memkv_get(store, key, strlen(key), &retrieved_value, &value_len);
    TEST_ASSERT_MSG(init_err == INFRA_ERROR_NOT_FOUND, "Key should not exist after deletion");

    // 再次检查统计信息
    stats = poly_memkv_get_stats(store);
    TEST_ASSERT_NOT_NULL(stats);
    TEST_ASSERT_EQUAL(2, poly_atomic_get(&stats->cmd_get));
    TEST_ASSERT_EQUAL(1, poly_atomic_get(&stats->cmd_set));
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
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 4096,
        .memory_limit = 1024 * 1024,  // 1MB memory limit
        .enable_compression = false
    };

    init_err = poly_memkv_create(&config, &store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create memkv instance");

    // 配置存储引擎
    init_err = poly_memkv_configure(store, &config);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to configure memkv instance");

    // 打开存储
    init_err = poly_memkv_open(store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to open memkv store");

    // 切换到 DuckDB 引擎
    config.engine = POLY_MEMKV_ENGINE_DUCKDB;
    config.url = ":memory:";  // 使用 DuckDB 内存数据库
    init_err = poly_memkv_switch_engine(store, POLY_MEMKV_ENGINE_DUCKDB, &config);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to switch to DuckDB engine");

    // 验证引擎类型
    TEST_ASSERT_EQUAL(POLY_MEMKV_ENGINE_DUCKDB, poly_memkv_get_engine_type(store));

    // 清理
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
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 0,  // 无效的键大小
        .max_value_size = 4096,
        .memory_limit = 1024 * 1024,  // 1MB memory limit
        .enable_compression = false
    };

    init_err = poly_memkv_create(&invalid_config, &store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create memkv instance");
    
    // 配置存储引擎（使用无效配置）
    init_err = poly_memkv_configure(store, &invalid_config);
    TEST_ASSERT_MSG(init_err == INFRA_ERROR_INVALID_PARAM, "Should fail with invalid key size");
    
    // 清理无效配置的实例
    if (store) {
        poly_memkv_destroy(store);
        store = NULL;
    }

    // 测试边界值
    poly_memkv_config_t valid_config = {
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 4096,
        .memory_limit = 1024 * 1024,  // 1MB memory limit
        .enable_compression = false
    };

    init_err = poly_memkv_create(&valid_config, &store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to create memkv instance with valid config");

    // 配置存储引擎（使用有效配置）
    init_err = poly_memkv_configure(store, &valid_config);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to configure memkv instance with valid config");

    // 打开存储
    init_err = poly_memkv_open(store);
    TEST_ASSERT_MSG(init_err == INFRA_OK, "Failed to open memkv store");

    // 测试超出大小限制
    char* large_key = infra_malloc(valid_config.max_key_size + 1);
    TEST_ASSERT_NOT_NULL(large_key);
    memset(large_key, 'A', valid_config.max_key_size);
    large_key[valid_config.max_key_size] = '\0';

    init_err = poly_memkv_set(store, large_key, strlen(large_key), "value", 5);
    TEST_ASSERT_MSG(init_err == INFRA_ERROR_INVALID_PARAM, "Should fail with key size exceeding limit");

    // 清理
    infra_free(large_key);
    poly_memkv_close(store);
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
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 4096,
        .memory_limit = 1024 * 1024,  // 1MB memory limit
        .enable_compression = false
    };

    init_err = poly_memkv_create(&config, &store);
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

// 添加新的测试用例
static void test_memkv_memory_limit(void) {
    poly_memkv_t* store = NULL;
    poly_memkv_config_t config = {
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 1024,
        .memory_limit = 2048,  // 很小的内存限制
        .enable_compression = false
    };

    infra_error_t err = poly_memkv_create(&config, &store);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to create memkv instance");

    // 尝试写入超过内存限制的数据
    char large_value[2048] = {0};
    memset(large_value, 'A', sizeof(large_value) - 1);
    
    err = poly_memkv_set(store, "large_key", large_value, sizeof(large_value));
    TEST_ASSERT_MSG(err == POLY_MEMKV_ERROR_MEMORY_LIMIT, "Should fail with memory limit error");

    poly_memkv_destroy(store);
}

static void test_memkv_compression(void) {
    poly_memkv_t* store = NULL;
    poly_memkv_config_t config = {
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",
        .max_key_size = 1024,
        .max_value_size = 4096,
        .memory_limit = 2048,
        .enable_compression = true
    };

    infra_error_t err = poly_memkv_create(&config, &store);
    TEST_ASSERT_MSG(err == INFRA_OK, "Failed to create memkv instance");

    // 创建可压缩的数据（重复模式）
    char compressible[2048] = {0};
    for (int i = 0; i < sizeof(compressible) - 1; i += 4) {
        memcpy(compressible + i, "ABCD", 4);
    }
    
    // 由于启用了压缩，这应该能成功
    err = poly_memkv_set(store, "compressed_key", compressible, sizeof(compressible));
    TEST_ASSERT_MSG(err == INFRA_OK, "Compression should allow large value to fit");

    poly_memkv_destroy(store);
}

int main(void) {
    TEST_BEGIN("Memory KV Store Tests");
    
    RUN_TEST(test_memkv_basic_ops);
    RUN_TEST(test_memkv_engine_switch);
    RUN_TEST(test_memkv_config);
    RUN_TEST(test_memkv_iterator);
    RUN_TEST(test_memkv_memory_limit);    // 新增
    RUN_TEST(test_memkv_compression);     // 新增
    
    TEST_END();
    return 0;
} 