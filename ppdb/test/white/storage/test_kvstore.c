/*
 * Copyright (c) 2024 PPDB Contributors
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */ 

#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_internal.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_sharded_memtable.h"
#include "kvstore/internal/kvstore_wal.h"
#include "ppdb/ppdb_logger.h"

// 测试配置
#define NUM_SHARDS 8
#define OPS_PER_THREAD 1000
#define NUM_THREADS 8
#define TABLE_SIZE (1024 * 1024)
#define KEY_SIZE 32
#define VALUE_SIZE 128
#define WAL_DIR "test_wal"

// 线程参数结构
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    bool success;
} thread_args_t;

// 线程工作函数
static void* concurrent_worker(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    args->success = true;
    
    for (int j = 0; j < OPS_PER_THREAD; j++) {
        char key[KEY_SIZE], value[VALUE_SIZE];
        snprintf(key, sizeof(key), "key_%d_%d", args->thread_id, j);
        snprintf(value, sizeof(value), "value_%d_%d_%s", args->thread_id, j, "padding_data_for_larger_value");

        // 写入
        ppdb_error_t err = ppdb_kvstore_put(args->store,
            (const void*)key, strlen(key),
            (const void*)value, strlen(value));
        if (err != PPDB_OK) {
            ppdb_log_error("Put operation failed in thread %d", args->thread_id);
            args->success = false;
            return NULL;
        }

        // 读取并验证
        void* read_value = NULL;
        size_t value_size = 0;
        err = ppdb_kvstore_get(args->store,
            (const void*)key, strlen(key),
            &read_value, &value_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Get operation failed in thread %d", args->thread_id);
            args->success = false;
            return NULL;
        }
        if (memcmp(read_value, value, strlen(value)) != 0) {
            ppdb_log_error("Value mismatch in thread %d", args->thread_id);
            args->success = false;
            free(read_value);
            return NULL;
        }
        free(read_value);

        // 随机删除一些键
        if (j % 3 == 0) {
            err = ppdb_kvstore_delete(args->store,
                (const void*)key, strlen(key));
            if (err != PPDB_OK) {
                ppdb_log_error("Delete operation failed in thread %d", args->thread_id);
                args->success = false;
                return NULL;
            }
        }
    }
    return NULL;
}

// 基本操作测试
static int test_basic_ops(void) {
    // 创建 KVStore 配置
    ppdb_kvstore_config_t config = {
        .memtable_size = TABLE_SIZE,
        .num_memtable_shards = NUM_SHARDS,
        .wal_dir = WAL_DIR,
        .sync_write = true
    };

    // 创建 KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Create KVStore failed");
    TEST_ASSERT(store != NULL, "KVStore is NULL");

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    err = ppdb_kvstore_put(store, 
        (const void*)test_key, strlen(test_key),
        (const void*)test_value, strlen(test_value));
    TEST_ASSERT(err == PPDB_OK, "Put operation failed");

    // 获取值
    void* value_buf = NULL;
    size_t actual_size = 0;
    err = ppdb_kvstore_get(store, 
        (const void*)test_key, strlen(test_key),
        &value_buf, &actual_size);
    TEST_ASSERT(err == PPDB_OK, "Get value failed");
    TEST_ASSERT(actual_size == strlen(test_value), "Value size mismatch");
    TEST_ASSERT(value_buf != NULL, "Value buffer is NULL");
    TEST_ASSERT(memcmp(value_buf, test_value, actual_size) == 0, "Value content mismatch");
    free(value_buf);

    // 测试删除
    err = ppdb_kvstore_delete(store, (const void*)test_key, strlen(test_key));
    TEST_ASSERT(err == PPDB_OK, "Delete operation failed");

    // 验证删除后无法获取
    err = ppdb_kvstore_get(store, 
        (const void*)test_key, strlen(test_key),
        &value_buf, &actual_size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist after delete");

    ppdb_kvstore_destroy(store);
    return 0;
}

// WAL持久化测试
static int test_wal_persistence(void) {
    // 创建第一个 KVStore 实例并写入数据
    ppdb_kvstore_config_t config = {
        .memtable_size = TABLE_SIZE,
        .num_memtable_shards = NUM_SHARDS,
        .wal_dir = WAL_DIR,
        .sync_write = true
    };

    ppdb_kvstore_t* store1 = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store1);
    TEST_ASSERT(err == PPDB_OK, "Create first KVStore failed");

    // 写入一些数据
    const int num_entries = 100;
    for (int i = 0; i < num_entries; i++) {
        char key[KEY_SIZE], value[VALUE_SIZE];
        snprintf(key, sizeof(key), "persist_key_%03d", i);
        snprintf(value, sizeof(value), "persist_value_%03d", i);

        err = ppdb_kvstore_put(store1,
            (const void*)key, strlen(key),
            (const void*)value, strlen(value));
        TEST_ASSERT(err == PPDB_OK, "Put operation failed");
    }

    // 关闭第一个实例
    ppdb_kvstore_destroy(store1);

    // 创建第二个实例，它应该从WAL中恢复数据
    ppdb_kvstore_t* store2 = NULL;
    err = ppdb_kvstore_create(&config, &store2);
    TEST_ASSERT(err == PPDB_OK, "Create second KVStore failed");

    // 验证所有数据都已恢复
    for (int i = 0; i < num_entries; i++) {
        char key[KEY_SIZE], expected_value[VALUE_SIZE];
        snprintf(key, sizeof(key), "persist_key_%03d", i);
        snprintf(expected_value, sizeof(expected_value), "persist_value_%03d", i);

        void* value = NULL;
        size_t value_size = 0;
        err = ppdb_kvstore_get(store2,
            (const void*)key, strlen(key),
            &value, &value_size);
        TEST_ASSERT(err == PPDB_OK, "Get operation failed after recovery");
        TEST_ASSERT(value_size == strlen(expected_value), "Value size mismatch after recovery");
        TEST_ASSERT(memcmp(value, expected_value, value_size) == 0, "Value content mismatch after recovery");
        free(value);
    }

    ppdb_kvstore_destroy(store2);
    return 0;
}

// 批量操作测试
static int test_batch_ops(void) {
    ppdb_kvstore_config_t config = {
        .memtable_size = TABLE_SIZE,
        .num_memtable_shards = NUM_SHARDS,
        .wal_dir = WAL_DIR,
        .sync_write = true
    };

    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Create KVStore failed");

    // 创建批量操作
    ppdb_batch_t* batch = NULL;
    err = ppdb_batch_create(&batch);
    TEST_ASSERT(err == PPDB_OK, "Create batch failed");

    // 添加一系列操作到批处理
    const int batch_size = 100;
    for (int i = 0; i < batch_size; i++) {
        char key[KEY_SIZE], value[VALUE_SIZE];
        snprintf(key, sizeof(key), "batch_key_%03d", i);
        snprintf(value, sizeof(value), "batch_value_%03d", i);

        if (i % 3 == 0) {
            // 删除操作
            err = ppdb_batch_delete(batch, (const void*)key, strlen(key));
        } else {
            // 写入操作
            err = ppdb_batch_put(batch,
                (const void*)key, strlen(key),
                (const void*)value, strlen(value));
        }
        TEST_ASSERT(err == PPDB_OK, "Batch operation addition failed");
    }

    // 执行批处理
    err = ppdb_kvstore_write_batch(store, batch);
    TEST_ASSERT(err == PPDB_OK, "Batch write failed");

    // 验证批处理结果
    for (int i = 0; i < batch_size; i++) {
        char key[KEY_SIZE], expected_value[VALUE_SIZE];
        snprintf(key, sizeof(key), "batch_key_%03d", i);
        snprintf(expected_value, sizeof(expected_value), "batch_value_%03d", i);

        void* value = NULL;
        size_t value_size = 0;
        err = ppdb_kvstore_get(store,
            (const void*)key, strlen(key),
            &value, &value_size);

        if (i % 3 == 0) {
            // 应该被删除的键
            TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should be deleted");
        } else {
            // 应该存在的键
            TEST_ASSERT(err == PPDB_OK, "Get operation failed");
            TEST_ASSERT(value_size == strlen(expected_value), "Value size mismatch");
            TEST_ASSERT(memcmp(value, expected_value, value_size) == 0, "Value content mismatch");
            free(value);
        }
    }

    ppdb_batch_destroy(batch);
    ppdb_kvstore_destroy(store);
    return 0;
}

// 并发操作测试
static int test_concurrent_ops(void) {
    ppdb_kvstore_config_t config = {
        .memtable_size = TABLE_SIZE,
        .num_memtable_shards = NUM_SHARDS,
        .wal_dir = WAL_DIR,
        .sync_write = true
    };

    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Create KVStore failed");

    pthread_t threads[NUM_THREADS];
    thread_args_t thread_args[NUM_THREADS];

    // 创建线程进行并发操作
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].store = store;
        thread_args[i].thread_id = i;
        thread_args[i].success = false;
        pthread_create(&threads[i], NULL, concurrent_worker, &thread_args[i]);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        TEST_ASSERT(thread_args[i].success, "Thread operation failed");
    }

    ppdb_kvstore_destroy(store);
    return 0;
}

// 迭代器测试
static int test_iterator(void) {
    ppdb_kvstore_config_t config = {
        .memtable_size = TABLE_SIZE,
        .num_memtable_shards = NUM_SHARDS,
        .wal_dir = WAL_DIR,
        .sync_write = true
    };

    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    TEST_ASSERT(err == PPDB_OK, "Create KVStore failed");

    // 插入有序的键值对
    const int num_entries = 100;
    for (int i = 0; i < num_entries; i++) {
        char key[KEY_SIZE], value[VALUE_SIZE];
        snprintf(key, sizeof(key), "iter_key_%03d", i);
        snprintf(value, sizeof(value), "iter_value_%03d", i);

        err = ppdb_kvstore_put(store,
            (const void*)key, strlen(key),
            (const void*)value, strlen(value));
        TEST_ASSERT(err == PPDB_OK, "Put operation failed");
    }

    // 创建迭代器
    ppdb_iterator_t* iter = NULL;
    err = ppdb_kvstore_iterator_create(store, &iter);
    TEST_ASSERT(err == PPDB_OK, "Create iterator failed");
    TEST_ASSERT(iter != NULL, "Iterator is NULL");

    // 验证迭代顺序
    int count = 0;
    while (ppdb_iterator_valid(iter)) {
        void* key = NULL;
        void* value = NULL;
        size_t key_size = 0;
        size_t value_size = 0;

        err = ppdb_iterator_get(iter, &key, &key_size, &value, &value_size);
        TEST_ASSERT(err == PPDB_OK, "Iterator get failed");

        char expected_key[KEY_SIZE];
        char expected_value[VALUE_SIZE];
        snprintf(expected_key, sizeof(expected_key), "iter_key_%03d", count);
        snprintf(expected_value, sizeof(expected_value), "iter_value_%03d", count);

        TEST_ASSERT(key_size == strlen(expected_key), "Key size mismatch");
        TEST_ASSERT(value_size == strlen(expected_value), "Value size mismatch");
        TEST_ASSERT(memcmp(key, expected_key, key_size) == 0, "Key content mismatch");
        TEST_ASSERT(memcmp(value, expected_value, value_size) == 0, "Value content mismatch");

        free(key);
        free(value);
        count++;
        ppdb_iterator_next(iter);
    }

    TEST_ASSERT(count == num_entries, "Iterator count mismatch");

    ppdb_iterator_destroy(iter);
    ppdb_kvstore_destroy(store);
    return 0;
}

int main(void) {
    test_framework_init();
    
    RUN_TEST(test_basic_ops);
    RUN_TEST(test_wal_persistence);
    RUN_TEST(test_batch_ops);
    RUN_TEST(test_concurrent_ops);
    RUN_TEST(test_iterator);
    
    test_print_stats();
    return test_get_result();
} 