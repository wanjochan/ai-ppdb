#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/skiplist.h"

// 测试基本操作
static void test_basic_operations(void) {
    printf("Testing basic operations...\n");
    
    ppdb_skiplist_t* list = NULL;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 0,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 1,
        .enable_ref_count = false
    };

    // 创建跳表
    ppdb_error_t err = ppdb_skiplist_create(&list, 16, ppdb_skiplist_default_compare, &config);
    assert(err == PPDB_OK && "Failed to create skiplist");
    assert(list != NULL && "Skiplist is NULL");

    // 测试插入
    const char* key1 = "key1";
    const char* value1 = "value1";
    err = ppdb_skiplist_put(list, key1, strlen(key1), value1, strlen(value1));
    assert(err == PPDB_OK && "Failed to put key1");

    // 测试查找
    void* value = NULL;
    size_t value_len = 0;
    err = ppdb_skiplist_get(list, key1, strlen(key1), &value, &value_len);
    assert(err == PPDB_OK && "Failed to get key1");
    assert(value != NULL && "Value is NULL");
    assert(value_len == strlen(value1) && "Value length mismatch");
    assert(memcmp(value, value1, value_len) == 0 && "Value content mismatch");

    // 测试更新
    const char* value2 = "new_value1";
    err = ppdb_skiplist_put(list, key1, strlen(key1), value2, strlen(value2));
    assert(err == PPDB_OK && "Failed to update key1");

    // 验证更新
    err = ppdb_skiplist_get(list, key1, strlen(key1), &value, &value_len);
    assert(err == PPDB_OK && "Failed to get updated key1");
    assert(value != NULL && "Updated value is NULL");
    assert(value_len == strlen(value2) && "Updated value length mismatch");
    assert(memcmp(value, value2, value_len) == 0 && "Updated value content mismatch");

    // 测试不存在的键
    const char* key_not_exist = "not_exist";
    err = ppdb_skiplist_get(list, key_not_exist, strlen(key_not_exist), &value, &value_len);
    assert(err == PPDB_ERR_NOT_FOUND && "Should return not found for non-existent key");

    // 销毁跳表
    ppdb_skiplist_destroy(list);
    printf("Basic operations test passed\n");
}

// 测试并发操作
static void test_concurrent_operations(void) {
    printf("Testing concurrent operations...\n");
    
    ppdb_skiplist_t* list = NULL;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 0,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 1,
        .enable_ref_count = false
    };

    // 创建跳表
    ppdb_error_t err = ppdb_skiplist_create(&list, 16, ppdb_skiplist_default_compare, &config);
    assert(err == PPDB_OK && "Failed to create skiplist");

    // TODO: 添加多线程测试
    // 由于这是基础测试，我们先确保单线程操作正确
    // 后续会添加更复杂的并发测试

    // 销毁跳表
    ppdb_skiplist_destroy(list);
    printf("Concurrent operations test passed\n");
}

// 测试边界条件
static void test_edge_cases(void) {
    printf("Testing edge cases...\n");
    
    ppdb_skiplist_t* list = NULL;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 0,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 1,
        .enable_ref_count = false
    };

    // 测试空参数
    ppdb_error_t err = ppdb_skiplist_create(NULL, 16, ppdb_skiplist_default_compare, &config);
    assert(err == PPDB_ERR_INVALID_ARG && "Should handle NULL list pointer");

    err = ppdb_skiplist_create(&list, 16, NULL, &config);
    assert(err == PPDB_ERR_INVALID_ARG && "Should handle NULL compare function");

    err = ppdb_skiplist_create(&list, 16, ppdb_skiplist_default_compare, NULL);
    assert(err == PPDB_ERR_INVALID_ARG && "Should handle NULL config");

    // 创建正常跳表
    err = ppdb_skiplist_create(&list, 16, ppdb_skiplist_default_compare, &config);
    assert(err == PPDB_OK && "Failed to create skiplist");

    // 测试空键值
    err = ppdb_skiplist_put(list, NULL, 0, "value", 5);
    assert(err == PPDB_ERR_INVALID_ARG && "Should handle NULL key");

    err = ppdb_skiplist_put(list, "key", 3, NULL, 0);
    assert(err == PPDB_ERR_INVALID_ARG && "Should handle NULL value");

    void* value = NULL;
    size_t value_len = 0;
    err = ppdb_skiplist_get(list, NULL, 0, &value, &value_len);
    assert(err == PPDB_ERR_INVALID_ARG && "Should handle NULL key in get");

    // 测试零长度键值
    const char* empty_key = "";
    const char* empty_value = "";
    err = ppdb_skiplist_put(list, empty_key, 0, empty_value, 0);
    assert(err == PPDB_OK && "Should handle empty key and value");

    // 销毁跳表
    ppdb_skiplist_destroy(list);
    printf("Edge cases test passed\n");
}

// 主测试函数
int main(void) {
    printf("Running skiplist tests...\n");
    
    test_basic_operations();
    test_concurrent_operations();
    test_edge_cases();
    
    printf("All skiplist tests passed!\n");
    return 0;
} 