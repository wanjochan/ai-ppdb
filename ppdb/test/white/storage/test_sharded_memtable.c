/*
 * Copyright (c) 2024 PPDB Contributors
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */ 

#include <cosmopolitan.h>
#include "../test_framework.h"
#include "internal/base.h"
#include "ppdb/ppdb.h"

// 测试配置
#define NUM_SHARDS 8
#define OPS_PER_THREAD 1000
#define NUM_THREADS 8
#define TABLE_SIZE (1024 * 1024)
#define KEY_SIZE 32
#define VALUE_SIZE 128

// 线程参数结构
typedef struct {
    ppdb_base_t* base;
    int thread_id;
    size_t num_ops;
} thread_args_t;

// 工作线程函数
static void* concurrent_worker(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char key_data[KEY_SIZE];
    char value_data[VALUE_SIZE];
    
    for (size_t i = 0; i < args->num_ops; i++) {
        // 生成键值对
        snprintf(key_data, sizeof(key_data), "key_%d_%zu", args->thread_id, i);
        snprintf(value_data, sizeof(value_data), "value_%d_%zu", args->thread_id, i);
        
        ppdb_key_t key = {key_data, strlen(key_data)};
        ppdb_value_t value = {value_data, strlen(value_data)};
        
        // 执行操作
        ppdb_error_t err = ppdb_put(args->base, &key, &value);
        if (err != PPDB_OK) {
            printf("Put operation failed in thread %d\n", args->thread_id);
            continue;
        }
        
        // 验证写入
        ppdb_value_t get_value = {NULL, 0};
        err = ppdb_get(args->base, &key, &get_value);
        if (err != PPDB_OK) {
            printf("Get operation failed in thread %d\n", args->thread_id);
            continue;
        }
        
        // 验证值
        if (get_value.size != value.size || 
            memcmp(get_value.data, value.data, value.size) != 0) {
            printf("Value mismatch in thread %d\n", args->thread_id);
        }
        
        // 清理
        if (get_value.data) {
            PPDB_ALIGNED_FREE(get_value.data);
        }
        
        // 删除一些键
        if (i % 3 == 0) {
            err = ppdb_remove(args->base, &key);
            if (err != PPDB_OK) {
                printf("Delete operation failed in thread %d\n", args->thread_id);
            }
        }
    }
    
    return NULL;
}

// 基本操作测试
static int test_basic_ops(void) {
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE | PPDB_FEAT_SHARDED,
        .shard_count = NUM_SHARDS,
        .memtable_size = TABLE_SIZE,
        .use_lockfree = true
    });
    ASSERT(err == PPDB_OK, "Failed to create sharded memtable");
    ASSERT(base != NULL, "Base pointer is NULL");
    
    // 测试基本操作
    char key_data[] = "test_key";
    char value_data[] = "test_value";
    ppdb_key_t key = {key_data, strlen(key_data)};
    ppdb_value_t value = {value_data, strlen(value_data)};
    
    // 插入
    err = ppdb_put(base, &key, &value);
    ASSERT(err == PPDB_OK, "Failed to put key-value pair");
    
    // 查询
    ppdb_value_t get_value = {NULL, 0};
    err = ppdb_get(base, &key, &get_value);
    ASSERT(err == PPDB_OK, "Failed to get value");
    ASSERT(get_value.size == value.size, "Value size mismatch");
    ASSERT(memcmp(get_value.data, value.data, value.size) == 0, "Value content mismatch");
    
    // 删除
    err = ppdb_remove(base, &key);
    ASSERT(err == PPDB_OK, "Failed to remove key");
    
    // 清理
    if (get_value.data) {
        PPDB_ALIGNED_FREE(get_value.data);
    }
    ppdb_destroy(base);
    return 0;
}

// 分片分布测试
static int test_shard_distribution(void) {
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE | PPDB_FEAT_SHARDED,
        .shard_count = NUM_SHARDS,
        .memtable_size = TABLE_SIZE,
        .use_lockfree = true
    });
    ASSERT(err == PPDB_OK, "Failed to create sharded memtable");
    
    // 测试键的分布
    size_t shard_counts[NUM_SHARDS] = {0};
    const size_t num_keys = 10000;
    
    for (size_t i = 0; i < num_keys; i++) {
        char key_data[32];
        snprintf(key_data, sizeof(key_data), "key_%zu", i);
        ppdb_key_t key = {key_data, strlen(key_data)};
        
        // 获取分片索引
        uint32_t shard_index = get_shard_index(&key, NUM_SHARDS);
        ASSERT(shard_index < NUM_SHARDS, "Invalid shard index");
        shard_counts[shard_index]++;
    }
    
    // 验证分布相对均匀
    const size_t expected_avg = num_keys / NUM_SHARDS;
    const size_t max_deviation = expected_avg / 2;
    
    for (size_t i = 0; i < NUM_SHARDS; i++) {
        printf("Shard %zu: %zu keys\n", i, shard_counts[i]);
        ASSERT(shard_counts[i] > expected_avg - max_deviation, 
               "Shard %zu has too few keys", i);
        ASSERT(shard_counts[i] < expected_avg + max_deviation,
               "Shard %zu has too many keys", i);
    }
    
    ppdb_destroy(base);
    return 0;
}

// 并发操作测试
static int test_concurrent_ops(void) {
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE | PPDB_FEAT_SHARDED,
        .shard_count = NUM_SHARDS,
        .memtable_size = TABLE_SIZE,
        .use_lockfree = true
    });
    ASSERT(err == PPDB_OK, "Failed to create sharded memtable");
    
    // 创建线程
    ppdb_base_thread_t* threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].base = base;
        args[i].thread_id = i;
        args[i].num_ops = OPS_PER_THREAD;
        
        err = ppdb_base_thread_create(&threads[i], concurrent_worker, &args[i]);
        ASSERT(err == PPDB_OK, "Failed to create thread %d", i);
    }
    
    // 等待线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        ppdb_base_thread_join(threads[i], NULL);
    }
    
    ppdb_destroy(base);
    return 0;
}

// 迭代器测试
static int test_iterator(void) {
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_SKIPLIST | PPDB_LAYER_MEMTABLE | PPDB_FEAT_SHARDED,
        .shard_count = NUM_SHARDS,
        .memtable_size = TABLE_SIZE,
        .use_lockfree = true
    });
    ASSERT(err == PPDB_OK, "Failed to create sharded memtable");
    
    // 插入一些数据
    const int num_items = 10;
    for (int i = 0; i < num_items; i++) {
        char key_data[32], value_data[32];
        snprintf(key_data, sizeof(key_data), "key_%d", i);
        snprintf(value_data, sizeof(value_data), "value_%d", i);
        
        ppdb_key_t key = {key_data, strlen(key_data)};
        ppdb_value_t value = {value_data, strlen(value_data)};
        
        err = ppdb_put(base, &key, &value);
        ASSERT(err == PPDB_OK, "Failed to put key-value pair %d", i);
    }
    
    // 使用迭代器遍历
    void* iter = NULL;
    err = ppdb_iterator_init(base, &iter);
    ASSERT(err == PPDB_OK, "Failed to create iterator");
    
    int count = 0;
    ppdb_key_t key;
    ppdb_value_t value;
    
    while ((err = ppdb_iterator_next(iter, &key, &value)) == PPDB_OK) {
        char expected_key[32], expected_value[32];
        snprintf(expected_key, sizeof(expected_key), "key_%d", count);
        snprintf(expected_value, sizeof(expected_value), "value_%d", count);
        
        printf("Actual key: %.*s (%zu bytes)\n", (int)key.size, (char*)key.data, key.size);
        printf("Actual value: %.*s (%zu bytes)\n\n", (int)value.size, (char*)value.data, value.size);
        
        ASSERT(key.size == strlen(expected_key), "Key size mismatch for item %d", count);
        ASSERT(value.size == strlen(expected_value), "Value size mismatch for item %d", count);
        ASSERT(memcmp(key.data, expected_key, key.size) == 0, "Key content mismatch for item %d", count);
        ASSERT(memcmp(value.data, expected_value, value.size) == 0, "Value content mismatch for item %d", count);
        
        count++;
        
        // 清理
        if (key.data) PPDB_ALIGNED_FREE(key.data);
        if (value.data) PPDB_ALIGNED_FREE(value.data);
    }
    
    ASSERT(count == num_items, "Iterator count mismatch: expected %d, got %d", num_items, count);
    ppdb_iterator_destroy(iter);
    ppdb_destroy(base);
    return 0;
}

int main(void) {
    test_framework_init();
    
    test_case_t test_cases[] = {
        {"test_basic_ops", "Test basic operations", test_basic_ops, 10, false},
        {"test_shard_distribution", "Test shard distribution", test_shard_distribution, 10, false},
        {"test_concurrent_ops", "Test concurrent operations", test_concurrent_ops, 30, false},
        {"test_iterator", "Test iterator", test_iterator, 10, false}
    };
    
    test_suite_t suite = {
        .name = "Sharded Memtable Tests",
        .setup = NULL,
        .teardown = NULL,
        .cases = test_cases,
        .num_cases = sizeof(test_cases) / sizeof(test_cases[0])
    };
    
    int result = run_test_suite(&suite);
    test_print_stats();
    test_framework_cleanup();
    return result;
} 