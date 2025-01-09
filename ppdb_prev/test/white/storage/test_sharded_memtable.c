/*
 * Copyright (c) 2024 PPDB Contributors
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */ 

#include <cosmopolitan.h>
#include "../test_framework.h"
#include "internal/base.h"
#include "internal/database.h"

// 测试配置
#define NUM_SHARDS 8
#define OPS_PER_THREAD 1000
#define NUM_THREADS 8
#define TABLE_SIZE (1024 * 1024)
#define KEY_SIZE 32
#define VALUE_SIZE 128

// 线程参数结构
typedef struct {
    ppdb_database_table_t* table;
    int thread_id;
    size_t num_ops;
} thread_args_t;

// 工作线程函数
static void concurrent_worker(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char key_data[KEY_SIZE];
    char value_data[VALUE_SIZE];
    
    for (size_t i = 0; i < args->num_ops; i++) {
        // 生成键值对
        snprintf(key_data, sizeof(key_data), "key_%d_%zu", args->thread_id, i);
        snprintf(value_data, sizeof(value_data), "value_%d_%zu", args->thread_id, i);
        
        // 执行操作
        ppdb_error_t err = ppdb_database_put(args->table, key_data, strlen(key_data), 
                                          value_data, strlen(value_data));
        if (err != PPDB_OK) {
            printf("Put operation failed in thread %d\n", args->thread_id);
            continue;
        }
        
        // 验证写入
        char result[VALUE_SIZE];
        size_t size = sizeof(result);
        err = ppdb_database_get(args->table, key_data, strlen(key_data), result, &size);
        if (err != PPDB_OK) {
            printf("Get operation failed in thread %d\n", args->thread_id);
            continue;
        }

        // 验证数据
        if (size != strlen(value_data) || memcmp(result, value_data, size) != 0) {
            printf("Data verification failed in thread %d\n", args->thread_id);
            continue;
        }
    }
}

// 基本操作测试
static void test_basic_ops(void) {
    ppdb_base_t* base = NULL;
    ppdb_database_t* database = NULL;
    ppdb_database_table_t* table = NULL;

    // 初始化基础层
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    TEST_ASSERT_EQUALS(ppdb_base_init(&base, &base_config), PPDB_OK);

    // 初始化数据库层
    ppdb_database_config_t database_config = {
        .memtable_size = TABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    TEST_ASSERT_EQUALS(ppdb_database_init(&database, base, &database_config), PPDB_OK);

    // 创建表
    TEST_ASSERT_EQUALS(ppdb_database_table_create(database, "test_table", &table), PPDB_OK);

    // 基本操作测试
    const char* key = "test_key";
    const char* value = "test_value";
    TEST_ASSERT_EQUALS(ppdb_database_put(table, key, strlen(key), value, strlen(value)), PPDB_OK);

    char result[256];
    size_t size = sizeof(result);
    TEST_ASSERT_EQUALS(ppdb_database_get(table, key, strlen(key), result, &size), PPDB_OK);
    TEST_ASSERT_EQUALS(memcmp(result, value, strlen(value)), 0);

    // 清理
    ppdb_database_table_destroy(table);
   ppdb_database_destroy(database);
    ppdb_base_destroy(base);
}

// 并发操作测试
static void test_concurrent_ops(void) {
    ppdb_base_t* base = NULL;
    ppdb_database_t* database = NULL;
    ppdb_database_table_t* table = NULL;
    ppdb_base_thread_t* threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];

    // 初始化基础层
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    TEST_ASSERT_EQUALS(ppdb_base_init(&base, &base_config), PPDB_OK);

    // 初始化数据库层
    ppdb_database_config_t database_config = {
        .memtable_size = TABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    TEST_ASSERT_EQUALS(ppdb_database_init(&database, base, &database_config), PPDB_OK);

    // 创建表
    TEST_ASSERT_EQUALS(ppdb_database_table_create(database, "test_table", &table), PPDB_OK);

    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].table = table;
        args[i].thread_id = i;
        args[i].num_ops = OPS_PER_THREAD;
        TEST_ASSERT_EQUALS(ppdb_base_thread_create(&threads[i], concurrent_worker, &args[i]), PPDB_OK);
    }

    // 等待线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_thread_join(threads[i], NULL), PPDB_OK);
    }

    // 清理
    ppdb_database_table_destroy(table);
   ppdb_database_destroy(database);
    ppdb_base_destroy(base);
}

int main(void) {
    printf("Running sharded memtable tests...\n");
    test_basic_ops();
    test_concurrent_ops();
    printf("All sharded memtable tests passed!\n");
    return 0;
} 