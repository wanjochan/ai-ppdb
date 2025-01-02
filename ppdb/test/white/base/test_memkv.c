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
    ppdb_status_t status = ppdb_memkv_create(&base, &config);
    
    ASSERT_TRUE(status == PPDB_OK);
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
    ppdb_status_t status = ppdb_memkv_create(&base, &config);
    ASSERT_TRUE(status == PPDB_OK);

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
    status = ppdb_put(base, &key, &value);
    ASSERT_TRUE(status == PPDB_OK);

    // Get
    ppdb_value_t get_value = {0};
    status = ppdb_get(base, &key, &get_value);
    ASSERT_TRUE(status == PPDB_OK);
    ASSERT_TRUE(get_value.size == value.size);
    ASSERT_TRUE(memcmp(get_value.data, value.data, value.size) == 0);

    // Remove
    status = ppdb_remove(base, &key);
    ASSERT_TRUE(status == PPDB_OK);

    // Get again (should fail)
    status = ppdb_get(base, &key, &get_value);
    ASSERT_TRUE(status == PPDB_NOT_FOUND);

    ppdb_destroy(base);
    return 0;
}

// 运行所有测试
int run_memkv_tests(void) {
    RUN_TEST(test_memkv_create);
    RUN_TEST(test_memkv_basic_ops);
    return 0;
}
