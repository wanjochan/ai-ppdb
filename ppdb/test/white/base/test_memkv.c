#include "../base/test_memkv.h"

// 全局变量
bool test_failed = false;

// 基本测试用例
int test_memkv_create(void) {
    ppdb_memkv_config_t config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .shard_count = 16,
        .bloom_bits = 10,
        .enable_stats = true
    };

    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_memkv_create(&base, &config);
    
    ASSERT_TRUE(err == PPDB_OK);
    ASSERT_TRUE(base != NULL);

    ppdb_destroy(base);
    return 0;
}

int test_memkv_basic_ops(void) {
    ppdb_memkv_config_t config = {
        .memory_limit = 1024 * 1024,
        .shard_count = 16,
        .enable_stats = true
    };

    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_memkv_create(&base, &config);
    ASSERT_TRUE(err == PPDB_OK);

    // 测试基本操作
    const char* key_str = "test_key";
    const char* value_str = "test_value";
    
    ppdb_key_t key = {
        .data = (uint8_t*)key_str,
        .size = strlen(key_str)
    };
    
    ppdb_value_t value = {
        .data = (uint8_t*)value_str,
        .size = strlen(value_str)
    };

    // Put
    err = ppdb_put(base, &key, &value);
    ASSERT_TRUE(err == PPDB_OK);

    // Get
    ppdb_value_t get_value = {0};
    err = ppdb_get(base, &key, &get_value);
    ASSERT_TRUE(err == PPDB_OK);
    ASSERT_TRUE(get_value.size == value.size);
    ASSERT_TRUE(memcmp(get_value.data, value.data, value.size) == 0);

    // Remove
    err = ppdb_remove(base, &key);
    ASSERT_TRUE(err == PPDB_OK);

    // Get again (should fail)
    err = ppdb_get(base, &key, &get_value);
    ASSERT_TRUE(err == PPDB_ERR_NOT_FOUND);

    // 测试状态信息
    ppdb_memkv_status_t status;
    err = ppdb_memkv_get_status(base, &status);
    ASSERT_TRUE(err == PPDB_OK);
    ASSERT_TRUE(status.item_count == 0);
    
    // 验证统计信息
    ASSERT_TRUE(status.stats.get_count == 2);      // 一次成功，一次失败
    ASSERT_TRUE(status.stats.get_hits == 1);       // 一次成功
    ASSERT_TRUE(status.stats.get_miss_count == 1); // 一次失败
    ASSERT_TRUE(status.stats.put_count == 1);      // 一次 put
    ASSERT_TRUE(status.stats.delete_count == 1);   // 一次 delete
    ASSERT_TRUE(status.stats.total_keys == 0);     // 已删除
    ASSERT_TRUE(status.stats.total_bytes == 0);    // 已删除

    ppdb_destroy(base);
    return 0;
}

// 运行所有测试
int run_memkv_tests(void) {
    RUN_TEST(test_memkv_create);
    RUN_TEST(test_memkv_basic_ops);
    return 0;
}
