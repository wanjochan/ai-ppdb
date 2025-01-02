#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_logger.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/skiplist.h"

// 测试基本操作
static int test_basic_operations(void) {
    PPDB_LOG_INFO("Testing basic operations...");
    
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
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");
    TEST_ASSERT(list != NULL, "Skiplist is NULL");

    // 测试插入
    const char* key1 = "key1";
    const char* value1 = "value1";
    err = ppdb_skiplist_put(list, key1, strlen(key1), value1, strlen(value1));
    TEST_ASSERT(err == PPDB_OK, "Failed to put key1");

    // 测试查找
    void* value = NULL;
    size_t value_len = 0;
    err = ppdb_skiplist_get(list, key1, strlen(key1), &value, &value_len);
    TEST_ASSERT(err == PPDB_OK, "Failed to get key1");
    TEST_ASSERT(value != NULL, "Value is NULL");
    TEST_ASSERT(value_len == strlen(value1), "Value length mismatch");
    TEST_ASSERT(memcmp(value, value1, value_len) == 0, "Value content mismatch");
    free(value);

    // 测试删除
    err = ppdb_skiplist_delete(list, key1, strlen(key1));
    TEST_ASSERT(err == PPDB_OK, "Failed to delete key1");

    // 验证删除
    err = ppdb_skiplist_get(list, key1, strlen(key1), &value, &value_len);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key1 still exists after deletion");

    // 清理
    ppdb_skiplist_destroy(list);
    return 0;
}

// 测试迭代器
static int test_iterator(void) {
    PPDB_LOG_INFO("Testing iterator...");
    
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
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");

    // 插入一些数据
    const char* keys[] = {"key1", "key2", "key3"};
    const char* values[] = {"value1", "value2", "value3"};
    const int num_entries = 3;

    for (int i = 0; i < num_entries; i++) {
        err = ppdb_skiplist_put(list, keys[i], strlen(keys[i]), values[i], strlen(values[i]));
        TEST_ASSERT(err == PPDB_OK, "Failed to put key");
    }

    // 创建迭代器
    ppdb_skiplist_iterator_t* iter = NULL;
    err = ppdb_skiplist_iterator_create(list, &iter, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to create iterator");

    // 遍历并验证
    int count = 0;
    ppdb_kv_pair_t pair = {0};
    while (ppdb_skiplist_iterator_valid(iter)) {
        err = ppdb_skiplist_iterator_get(iter, &pair);
        TEST_ASSERT(err == PPDB_OK, "Failed to get from iterator");
        TEST_ASSERT(pair.key_size == strlen(keys[count]), "Key length mismatch");
        TEST_ASSERT(pair.value_size == strlen(values[count]), "Value length mismatch");
        TEST_ASSERT(memcmp(pair.key, keys[count], pair.key_size) == 0, "Key content mismatch");
        TEST_ASSERT(memcmp(pair.value, values[count], pair.value_size) == 0, "Value content mismatch");

        count++;
        err = ppdb_skiplist_iterator_next(iter, &pair);
        TEST_ASSERT(err == PPDB_OK, "Failed to move iterator");
    }

    TEST_ASSERT(count == num_entries, "Iterator count mismatch");

    // 清理
    ppdb_skiplist_iterator_destroy(iter);
    ppdb_skiplist_destroy(list);
    return 0;
}

// 测试并发操作
static void* concurrent_worker(void* arg) {
    ppdb_skiplist_t* list = (ppdb_skiplist_t*)arg;
    char key[32];
    char value[32];

    for (int i = 0; i < 1000; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);

        // 插入
        ppdb_error_t err = ppdb_skiplist_put(list, key, strlen(key), value, strlen(value));
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Failed to put in concurrent test");
            return (void*)-1;
        }

        // 查找
        void* read_value = NULL;
        size_t value_len = 0;
        err = ppdb_skiplist_get(list, key, strlen(key), &read_value, &value_len);
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Failed to get in concurrent test");
            return (void*)-1;
        }
        free(read_value);

        // 删除
        err = ppdb_skiplist_delete(list, key, strlen(key));
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Failed to delete in concurrent test");
            return (void*)-1;
        }
    }

    return NULL;
}

static int test_concurrent_operations(void) {
    PPDB_LOG_INFO("Testing concurrent operations...");
    
    ppdb_skiplist_t* list = NULL;
    const char* test_mode = getenv("PPDB_SYNC_MODE");
    bool use_lockfree = (test_mode && strcmp(test_mode, "lockfree") == 0);
    
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 10000,
        .use_lockfree = use_lockfree,
        .stripe_count = 16,
        .backoff_us = use_lockfree ? 1 : 100,
        .enable_ref_count = true,
        .retry_count = 100,
        .retry_delay_us = 1
    };

    // 创建跳表
    ppdb_error_t err = ppdb_skiplist_create(&list, 16, ppdb_skiplist_default_compare, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");

    // 创建线程
    const int num_threads = 4;
    pthread_t threads[num_threads];
    
    for (int i = 0; i < num_threads; i++) {
        int ret = pthread_create(&threads[i], NULL, concurrent_worker, list);
        TEST_ASSERT(ret == 0, "Failed to create thread");
    }

    // 等待线程完成
    for (int i = 0; i < num_threads; i++) {
        void* result;
        int ret = pthread_join(threads[i], &result);
        TEST_ASSERT(ret == 0, "Failed to join thread");
        TEST_ASSERT(result == NULL, "Thread reported error");
    }

    // 清理
    ppdb_skiplist_destroy(list);
    return 0;
}

int main(void) {
    TEST_INIT();
    PPDB_LOG_INFO("Running Skiplist Tests...");
    
    RUN_TEST(test_basic_operations);
    RUN_TEST(test_iterator);
    RUN_TEST(test_concurrent_operations);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 