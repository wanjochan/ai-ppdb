#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "ppdb/ppdb_logger.h"

// 测试配置
#define OPS_PER_THREAD 100
#define NUM_THREADS 4
#define TABLE_SIZE (1024 * 1024)

// 线程参数结构
typedef struct {
    ppdb_memtable_t* table;
    int thread_id;
    bool success;
} thread_args_t;

// 线程工作函数
static void* concurrent_worker(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    args->success = true;
    
    for (int j = 0; j < OPS_PER_THREAD; j++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "key_%d_%d", args->thread_id, j);
        snprintf(value, sizeof(value), "value_%d_%d", args->thread_id, j);

        // 写入
        ppdb_error_t err = ppdb_memtable_put(args->table,
            (const void*)key, strlen(key),
            (const void*)value, strlen(value));
        if (err != PPDB_OK) {
            ppdb_log_error("Put operation failed");
            args->success = false;
            return NULL;
        }

        // 读取并验证
        void* read_value = NULL;
        size_t value_size = 0;
        err = ppdb_memtable_get(args->table,
            (const void*)key, strlen(key),
            &read_value, &value_size);
        if (err != PPDB_OK) {
            ppdb_log_error("Get operation failed");
            args->success = false;
            return NULL;
        }
        if (memcmp(read_value, value, strlen(value)) != 0) {
            ppdb_log_error("Value mismatch");
            args->success = false;
            free(read_value);
            return NULL;
        }
        free(read_value);

        // 随机删除一些键
        if (j % 3 == 0) {
            err = ppdb_memtable_delete(args->table,
                (const void*)key, strlen(key));
            if (err != PPDB_OK) {
                ppdb_log_error("Delete operation failed");
                args->success = false;
                return NULL;
            }
        }
    }
    return NULL;
}

// 基本操作测试
static int test_basic_ops(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(TABLE_SIZE, &table);
    TEST_ASSERT(err == PPDB_OK, "Create memtable failed");
    TEST_ASSERT(table != NULL, "Memtable is NULL");

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    err = ppdb_memtable_put(table, (const void*)test_key, strlen(test_key),
                           (const void*)test_value, strlen(test_value));
    TEST_ASSERT(err == PPDB_OK, "Put operation failed");

    // 先获取值的大小
    size_t value_size = 0;
    err = ppdb_memtable_get(table, (const void*)test_key, strlen(test_key),
                           NULL, &value_size);
    TEST_ASSERT(err == PPDB_OK, "Get size failed");
    TEST_ASSERT(value_size == strlen(test_value), "Value size mismatch");

    // 获取值
    void* value_buf = NULL;
    size_t actual_size = 0;
    err = ppdb_memtable_get(table, (const void*)test_key, strlen(test_key),
                           &value_buf, &actual_size);
    TEST_ASSERT(err == PPDB_OK, "Get value failed");
    TEST_ASSERT(actual_size == strlen(test_value), "Value size mismatch");
    TEST_ASSERT(value_buf != NULL, "Value buffer is NULL");
    TEST_ASSERT(memcmp(value_buf, test_value, actual_size) == 0, "Value content mismatch");
    free(value_buf);

    // 测试删除
    err = ppdb_memtable_delete(table, (const void*)test_key, strlen(test_key));
    TEST_ASSERT(err == PPDB_OK, "Delete operation failed");

    // 验证删除后无法获取
    err = ppdb_memtable_get(table, (const void*)test_key, strlen(test_key),
                           NULL, &value_size);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist after delete");

    ppdb_memtable_destroy(table);
    return 0;
}

// 分片测试
static int test_sharding(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(TABLE_SIZE, &table);
    TEST_ASSERT(err == PPDB_OK, "Create memtable failed");
    TEST_ASSERT(table != NULL, "Memtable is NULL");

    // 测试多个分片的并发写入
    #define NUM_KEYS 1000
    #define KEY_SIZE 16
    #define VALUE_SIZE 100

    // 并发写入不同分片
    for (int i = 0; i < NUM_KEYS; i++) {
        char key[KEY_SIZE];
        char value[VALUE_SIZE];
        snprintf(key, sizeof(key), "key_%04d", i);
        memset(value, 'v', VALUE_SIZE-1);
        value[VALUE_SIZE-1] = '\0';

        err = ppdb_memtable_put(table, 
            (const void*)key, strlen(key),
            (const void*)value, strlen(value));
        TEST_ASSERT(err == PPDB_OK, "Put operation failed");
    }

    // 验证所有写入
    for (int i = 0; i < NUM_KEYS; i++) {
        char key[KEY_SIZE];
        snprintf(key, sizeof(key), "key_%04d", i);
        
        void* value = NULL;
        size_t value_size = 0;
        err = ppdb_memtable_get(table, (const void*)key, strlen(key),
                               &value, &value_size);
        TEST_ASSERT(err == PPDB_OK, "Get operation failed");
        TEST_ASSERT(value_size == VALUE_SIZE-1, "Value size mismatch");
        free(value);
    }

    ppdb_memtable_destroy(table);
    return 0;
}

// 并发操作测试
static int test_concurrent_ops(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(TABLE_SIZE, &table);
    TEST_ASSERT(err == PPDB_OK, "Create memtable failed");

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
        TEST_ASSERT(thread_args[i].success, "Thread operation failed");
    }

    ppdb_memtable_destroy(table);
    return 0;
}

int main(void) {
    test_framework_init();
    
    RUN_TEST(test_basic_ops);
    RUN_TEST(test_sharding);
    RUN_TEST(test_concurrent_ops);
    
    test_print_stats();
    return test_get_result();
} 