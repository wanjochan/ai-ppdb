#include <cosmopolitan.h>
#include "test_framework.h"
#include "kvstore/internal/kvstore_memtable.h"

// 基本操作测试
void test_basic_ops(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(table);

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    err = ppdb_memtable_put(table, (const uint8_t*)test_key, strlen(test_key),
                           (const uint8_t*)test_value, strlen(test_value));
    ASSERT_EQ(err, PPDB_OK);

    // 先获取值的大小
    size_t value_size = 0;
    err = ppdb_memtable_get(table, (const uint8_t*)test_key, strlen(test_key),
                           NULL, &value_size);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_EQ(value_size, strlen(test_value));

    // 获取值
    uint8_t* value_buf = NULL;
    size_t actual_size = 0;
    err = ppdb_memtable_get(table, (const uint8_t*)test_key, strlen(test_key),
                           &value_buf, &actual_size);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_EQ(actual_size, strlen(test_value));
    ASSERT_NOT_NULL(value_buf);
    ASSERT_MEM_EQ(value_buf, test_value, actual_size);
    free(value_buf);

    // 测试删除
    err = ppdb_memtable_delete(table, (const uint8_t*)test_key, strlen(test_key));
    ASSERT_EQ(err, PPDB_OK);

    // 验证删除后无法获取
    err = ppdb_memtable_get(table, (const uint8_t*)test_key, strlen(test_key),
                           NULL, &value_size);
    ASSERT_EQ(err, PPDB_ERR_NOT_FOUND);

    ppdb_memtable_destroy(table);
}

// 分片测试
void test_sharding(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create_sharded(4096, 4, &table);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_NOT_NULL(table);

    // 测试多个分片的并发写入
    #define NUM_KEYS 1000
    #define KEY_SIZE 16
    #define VALUE_SIZE 100

    // 并发写入不同分片
    #pragma omp parallel for
    for (int i = 0; i < NUM_KEYS; i++) {
        char key[KEY_SIZE];
        char value[VALUE_SIZE];
        snprintf(key, sizeof(key), "key_%04d", i);
        memset(value, 'v', VALUE_SIZE-1);
        value[VALUE_SIZE-1] = '\0';

        ppdb_error_t put_err = ppdb_memtable_put(table, 
            (const uint8_t*)key, strlen(key),
            (const uint8_t*)value, strlen(value));
        ASSERT_EQ(put_err, PPDB_OK);
    }

    // 验证所有写入
    for (int i = 0; i < NUM_KEYS; i++) {
        char key[KEY_SIZE];
        snprintf(key, sizeof(key), "key_%04d", i);
        
        uint8_t* value = NULL;
        size_t value_size = 0;
        err = ppdb_memtable_get(table, (const uint8_t*)key, strlen(key),
                               &value, &value_size);
        ASSERT_EQ(err, PPDB_OK);
        ASSERT_EQ(value_size, VALUE_SIZE-1);
        free(value);
    }

    ppdb_memtable_destroy(table);
}

// 并发操作测试
void test_concurrent_ops(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(4096, &table);
    ASSERT_EQ(err, PPDB_OK);

    // 测试并发读写
    #define NUM_THREADS 4
    #define OPS_PER_THREAD 1000

    pthread_t threads[NUM_THREADS];
    struct {
        ppdb_memtable_t* table;
        int thread_id;
    } thread_args[NUM_THREADS];

    // 创建线程进行并发操作
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].table = table;
        thread_args[i].thread_id = i;
        pthread_create(&threads[i], NULL, [](void* arg) -> void* {
            auto* args = (typeof(thread_args))arg;
            
            for (int j = 0; j < OPS_PER_THREAD; j++) {
                char key[32], value[32];
                snprintf(key, sizeof(key), "key_%d_%d", args->thread_id, j);
                snprintf(value, sizeof(value), "value_%d_%d", args->thread_id, j);

                // 写入
                ppdb_error_t err = ppdb_memtable_put(args->table,
                    (const uint8_t*)key, strlen(key),
                    (const uint8_t*)value, strlen(value));
                ASSERT_EQ(err, PPDB_OK);

                // 读取并验证
                uint8_t* read_value = NULL;
                size_t value_size = 0;
                err = ppdb_memtable_get(args->table,
                    (const uint8_t*)key, strlen(key),
                    &read_value, &value_size);
                ASSERT_EQ(err, PPDB_OK);
                ASSERT_MEM_EQ(read_value, value, strlen(value));
                free(read_value);

                // 随机删除一些键
                if (j % 3 == 0) {
                    err = ppdb_memtable_delete(args->table,
                        (const uint8_t*)key, strlen(key));
                    ASSERT_EQ(err, PPDB_OK);
                }
            }
            return NULL;
        }, &thread_args[i]);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    ppdb_memtable_destroy(table);
}

int main(void) {
    TEST_INIT("Memory Table Test");
    
    RUN_TEST(test_basic_ops);
    RUN_TEST(test_sharding);
    RUN_TEST(test_concurrent_ops);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 