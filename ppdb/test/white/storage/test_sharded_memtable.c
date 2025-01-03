/*
 * Copyright (c) 2024 PPDB Contributors
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */ 

#include <cosmopolitan.h>
#include "../test_framework.h"
#include "ppdb/ppdb.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_sharded_memtable.h"

// 测试配置
#define NUM_SHARDS 8
#define OPS_PER_THREAD 1000
#define NUM_THREADS 8
#define TABLE_SIZE (1024 * 1024)
#define KEY_SIZE 32
#define VALUE_SIZE 128

// 线程参数结构
typedef struct {
    ppdb_sharded_memtable_t* table;
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
        ppdb_error_t err = ppdb_sharded_memtable_put(args->table,
            (const void*)key, strlen(key),
            (const void*)value, strlen(value));
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Put operation failed in thread %d", args->thread_id);
            args->success = false;
            return NULL;
        }

        // 读取并验证
        void* read_value = NULL;
        size_t value_size = 0;
        err = ppdb_sharded_memtable_get(args->table,
            (const void*)key, strlen(key),
            &read_value, &value_size);
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Get operation failed in thread %d", args->thread_id);
            args->success = false;
            return NULL;
        }

        if (value_size != strlen(value) || memcmp(read_value, value, value_size) != 0) {
            PPDB_LOG_ERROR("Value mismatch in thread %d", args->thread_id);
            args->success = false;
            free(read_value);
            return NULL;
        }
        free(read_value);

        // 随机删除一些键
        if (j % 3 == 0) {
            err = ppdb_sharded_memtable_delete(args->table,
                (const void*)key, strlen(key));
            if (err != PPDB_OK) {
                PPDB_LOG_ERROR("Delete operation failed in thread %d", args->thread_id);
                args->success = false;
                return NULL;
            }
        }
    }
    return NULL;
}

// 基本操作测试
static int test_basic_ops(void) {
    ppdb_sharded_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_sharded_memtable_create(&table, NUM_SHARDS);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(table);

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    err = ppdb_sharded_memtable_put(table, 
        (const void*)test_key, strlen(test_key),
        (const void*)test_value, strlen(test_value));
    ASSERT_EQ(err, PPDB_OK);

    // 获取值
    void* value_buf = NULL;
    size_t actual_size = 0;
    err = ppdb_sharded_memtable_get(table, 
        (const void*)test_key, strlen(test_key),
        &value_buf, &actual_size);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_EQ(actual_size, strlen(test_value));
    ASSERT_NOT_NULL(value_buf);
    ASSERT_EQ(memcmp(value_buf, test_value, actual_size), 0);
    free(value_buf);

    // 测试删除
    err = ppdb_sharded_memtable_delete(table, (const void*)test_key, strlen(test_key));
    ASSERT_EQ(err, PPDB_OK);

    // 验证删除后无法获取
    err = ppdb_sharded_memtable_get(table, 
        (const void*)test_key, strlen(test_key),
        &value_buf, &actual_size);
    ASSERT_EQ(err, PPDB_ERR_NOT_FOUND);

    ppdb_sharded_memtable_destroy(table);
    return 0;
}

// 分片均衡性测试
static int test_shard_distribution(void) {
    ppdb_sharded_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_sharded_memtable_create(&table, NUM_SHARDS);
    ASSERT_EQ(err, PPDB_OK);

    // 写入大量数据以测试分片分布
    #define DIST_TEST_KEYS 10000
    size_t shard_counts[NUM_SHARDS] = {0};
    
    for (int i = 0; i < DIST_TEST_KEYS; i++) {
        char key[KEY_SIZE];
        char value[VALUE_SIZE];
        snprintf(key, sizeof(key), "dist_key_%d", i);
        snprintf(value, sizeof(value), "dist_value_%d", i);

        err = ppdb_sharded_memtable_put(table,
            (const void*)key, strlen(key),
            (const void*)value, strlen(value));
        ASSERT_EQ(err, PPDB_OK);

        // 获取键所在的分片并计数
        size_t shard_index = ppdb_sharded_memtable_get_shard_index(table, 
            (const void*)key, strlen(key));
        ASSERT_GT(NUM_SHARDS, shard_index);
        shard_counts[shard_index]++;
    }

    // 检查分片分布的均衡性
    size_t expected_per_shard = DIST_TEST_KEYS / NUM_SHARDS;
    size_t variance_threshold = expected_per_shard * 0.3; // 允许30%的方差

    for (int i = 0; i < NUM_SHARDS; i++) {
        size_t diff = (shard_counts[i] > expected_per_shard) ? 
            (shard_counts[i] - expected_per_shard) : 
            (expected_per_shard - shard_counts[i]);
            
        ASSERT_GT(variance_threshold + 1, diff);
    }

    ppdb_sharded_memtable_destroy(table);
    return 0;
}

// 并发操作测试
static int test_concurrent_ops(void) {
    ppdb_sharded_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_sharded_memtable_create(&table, NUM_SHARDS);
    ASSERT_EQ(err, PPDB_OK);

    pthread_t threads[NUM_THREADS];
    thread_args_t thread_args[NUM_THREADS];

    // 创建线程进行并发操作
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].table = table;
        thread_args[i].thread_id = i;
        thread_args[i].success = false;
        pthread_create(&threads[i], NULL, concurrent_worker, &thread_args[i]);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        ASSERT_EQ(thread_args[i].success, true);
    }

    ppdb_sharded_memtable_destroy(table);
    return 0;
}

// 迭代器测试
static int test_iterator(void) {
    ppdb_sharded_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_sharded_memtable_create(&table, NUM_SHARDS);
    ASSERT_EQ(err, PPDB_OK);

    // 插入有序的键值对
    const int num_entries = 100;
    for (int i = 0; i < num_entries; i++) {
        char key[KEY_SIZE], value[VALUE_SIZE];
        snprintf(key, sizeof(key), "iter_key_%03d", i);
        snprintf(value, sizeof(value), "iter_value_%03d", i);

        err = ppdb_sharded_memtable_put(table,
            (const void*)key, strlen(key),
            (const void*)value, strlen(value));
        ASSERT_EQ(err, PPDB_OK);
        printf("Inserted key: %s, value: %s\n", key, value);
    }

    // 创建迭代器
    ppdb_iterator_t* iter = NULL;
    err = ppdb_sharded_memtable_iterator_create(table, &iter);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(iter);

    // 验证迭代顺序
    int count = 0;
    while (iter->valid(iter)) {
        ppdb_kv_pair_t pair;
        err = iter->get(iter, &pair);
        ASSERT_EQ(err, PPDB_OK);

        char expected_key[KEY_SIZE];
        char expected_value[VALUE_SIZE];
        snprintf(expected_key, sizeof(expected_key), "iter_key_%03d", count);
        snprintf(expected_value, sizeof(expected_value), "iter_value_%03d", count);

        printf("Count: %d\n", count);
        printf("Expected key: %s (%zu bytes)\n", expected_key, strlen(expected_key));
        printf("Actual key: %.*s (%zu bytes)\n", (int)pair.key_size, (char*)pair.key, pair.key_size);
        printf("Expected value: %s (%zu bytes)\n", expected_value, strlen(expected_value));
        printf("Actual value: %.*s (%zu bytes)\n\n", (int)pair.value_size, (char*)pair.value, pair.value_size);

        ASSERT_EQ(pair.key_size, strlen(expected_key));
        ASSERT_EQ(pair.value_size, strlen(expected_value));
        ASSERT_EQ(memcmp(pair.key, expected_key, pair.key_size), 0);
        ASSERT_EQ(memcmp(pair.value, expected_value, pair.value_size), 0);

        // 释放当前键值对的内存
        free(pair.key);
        free(pair.value);

        count++;
        iter->next(iter);
    }

    ASSERT_EQ(count, num_entries);

    ppdb_iterator_destroy(iter);
    ppdb_sharded_memtable_destroy(table);
    return 0;
}

int main(void) {
    test_framework_init();
    
    RUN_TEST(test_basic_ops);
    RUN_TEST(test_shard_distribution);
    RUN_TEST(test_concurrent_ops);
    RUN_TEST(test_iterator);
    
    test_print_stats();
    return test_get_result();
} 