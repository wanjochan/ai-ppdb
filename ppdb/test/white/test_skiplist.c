#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_sync.h"
#include "ppdb/kvstore/skiplist.h"
#include "test/white/test_framework.h"

// 测试基本操作
static int test_basic_operations(void) {
    printf("Starting basic operations test\n");
    
    ppdb_skiplist_t* list = NULL;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = false,
        .stripe_count = 1,
        .spin_count = 1000,
        .backoff_us = 100,
        .enable_ref_count = false,
        .retry_count = 100,
        .retry_delay_us = 1
    };

    printf("Creating skiplist...\n");
    // 创建跳表
    ppdb_error_t err = ppdb_skiplist_create(&list, 16, ppdb_skiplist_default_compare, &config);
    printf("Create result: %d\n", err);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");
    TEST_ASSERT(list != NULL, "Skiplist is NULL");

    printf("Testing empty skiplist state...\n");
    // 测试空跳表状态
    bool is_empty = ppdb_skiplist_empty(list);
    size_t size = ppdb_skiplist_size(list);
    printf("Empty: %d, Size: %zu\n", is_empty, size);
    TEST_ASSERT(is_empty, "New skiplist should be empty");
    TEST_ASSERT(size == 0, "New skiplist size should be 0");

    printf("Testing insertion...\n");
    // 测试插入
    const char* key = "test_key";
    const char* value = "test_value";
    size_t key_len = strlen(key);
    size_t value_len = strlen(value);
    printf("key_len: %zu, value_len: %zu\n", key_len, value_len);
    err = ppdb_skiplist_put(list, key, key_len, value, value_len);
    printf("Insert result: %d\n", err);
    TEST_ASSERT(err == PPDB_OK, "Failed to put key");

    is_empty = ppdb_skiplist_empty(list);
    size = ppdb_skiplist_size(list);
    printf("After insert - Empty: %d, Size: %zu\n", is_empty, size);
    TEST_ASSERT(!is_empty, "Skiplist should not be empty after insert");
    TEST_ASSERT(size == 1, "Skiplist size should be 1 after insert");

    printf("Testing retrieval...\n");
    // 测试获取
    void* result = NULL;
    size_t result_len = 0;
    err = ppdb_skiplist_get(list, key, key_len, &result, &result_len);
    printf("Get result: %d, result_len: %zu\n", err, result_len);
    TEST_ASSERT(err == PPDB_OK, "Failed to get key");
    TEST_ASSERT(result != NULL, "Result is NULL");
    TEST_ASSERT(result_len == value_len, "Value length mismatch");
    if (result) {
        printf("Retrieved value: %s\n", (char*)result);
        TEST_ASSERT(memcmp(value, result, value_len) == 0, "Value mismatch");
        free(result);
    }

    printf("Testing update...\n");
    // 测试更新
    const char* new_value = "updated_value";
    size_t new_value_len = strlen(new_value);
    err = ppdb_skiplist_put(list, key, key_len, new_value, new_value_len);
    printf("Update result: %d\n", err);
    TEST_ASSERT(err == PPDB_OK, "Failed to update key");

    size = ppdb_skiplist_size(list);
    printf("After update - Size: %zu\n", size);
    TEST_ASSERT(size == 1, "Skiplist size should not change after update");

    printf("Testing deletion...\n");
    // 测试删除
    err = ppdb_skiplist_delete(list, key, key_len);
    printf("Delete result: %d\n", err);
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key");

    is_empty = ppdb_skiplist_empty(list);
    size = ppdb_skiplist_size(list);
    printf("After delete - Empty: %d, Size: %zu\n", is_empty, size);
    TEST_ASSERT(is_empty, "Skiplist should be empty after delete");
    TEST_ASSERT(size == 0, "Skiplist size should be 0 after delete");

    printf("Testing non-existent key...\n");
    // 测试获取不存在的键
    err = ppdb_skiplist_get(list, key, key_len, &result, &result_len);
    printf("Get non-existent key result: %d\n", err);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key still exists after deletion");

    printf("Destroying skiplist...\n");
    // 销毁跳表
    ppdb_skiplist_destroy(list);
    printf("Test completed\n");
    return 0;
}

// 测试迭代器
static int test_iterator(void) {
    printf("Starting iterator test\n");
    ppdb_skiplist_t* list = NULL;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = false,
        .stripe_count = 1,
        .spin_count = 1000,
        .backoff_us = 100,
        .enable_ref_count = false,
        .retry_count = 100,
        .retry_delay_us = 1
    };

    // 创建跳表
    printf("Creating skiplist...\n");
    ppdb_error_t err = ppdb_skiplist_create(&list, 16, ppdb_skiplist_default_compare, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");

    // 插入测试数据
    printf("Inserting test data...\n");
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    for (int i = 0; i < 3; i++) {
        size_t key_len = strlen(keys[i]);
        size_t value_len = strlen(values[i]);
        printf("Inserting key: %s (len=%zu), value: %s (len=%zu)\n", 
               keys[i], key_len, values[i], value_len);
        err = ppdb_skiplist_put(list, keys[i], key_len, values[i], value_len);
        TEST_ASSERT(err == PPDB_OK, "Failed to put key");
    }

    // 创建迭代器
    printf("Creating iterator...\n");
    ppdb_skiplist_iterator_t* iter = NULL;
    err = ppdb_skiplist_iterator_create(list, &iter, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to create iterator");
    TEST_ASSERT(iter != NULL, "Iterator is NULL");

    // 遍历并验证
    printf("Iterating through data...\n");
    int count = 0;
    while (ppdb_skiplist_iterator_valid(iter)) {
        count++;  // 在获取数据之前增加计数
        
        ppdb_kv_pair_t pair;
        err = ppdb_skiplist_iterator_get(iter, &pair);
        TEST_ASSERT(err == PPDB_OK, "Failed to get from iterator");
        TEST_ASSERT(pair.key != NULL, "Key is NULL");
        TEST_ASSERT(pair.value != NULL, "Value is NULL");
        
        printf("Retrieved key: %.*s, value: %.*s\n", 
               (int)pair.key_size, (char*)pair.key,
               (int)pair.value_size, (char*)pair.value);
        
        // 释放获取的键值对
        free(pair.key);
        free(pair.value);
        
        err = ppdb_skiplist_iterator_next(iter);
        if (err == PPDB_ERR_NOT_FOUND) break;
        TEST_ASSERT(err == PPDB_OK, "Failed to move iterator");
    }
    printf("Iteration completed, count=%d\n", count);
    TEST_ASSERT(count == 3, "Iterator count mismatch");

    // 销毁迭代器
    printf("Destroying iterator...\n");
    ppdb_skiplist_iterator_destroy(iter);
    
    // 销毁跳表
    printf("Destroying skiplist...\n");
    ppdb_skiplist_destroy(list);
    printf("Test completed\n");
    return 0;
}

// 测试并发操作
static int test_concurrent_operations(void) {
    ppdb_skiplist_t* list = NULL;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = false,
        .stripe_count = 1,
        .spin_count = 1000,
        .backoff_us = 100,
        .enable_ref_count = false,
        .retry_count = 100,
        .retry_delay_us = 1
    };

    // 创建跳表
    ppdb_error_t err = ppdb_skiplist_create(&list, 16, ppdb_skiplist_default_compare, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");

    // TODO: 添加并发测试用例

    // 销毁跳表
    ppdb_skiplist_destroy(list);
    return 0;
}

int main(void) {
    TEST_INIT();
    
    RUN_TEST(test_basic_operations);
    RUN_TEST(test_iterator);
    RUN_TEST(test_concurrent_operations);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 