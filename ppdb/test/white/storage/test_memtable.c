#include <cosmopolitan.h>
#include "../test_framework.h"
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_sync.h"
#include "kvstore/internal/kvstore_memtable.h"

// 测试配置
#define OPS_PER_THREAD 100
#define NUM_THREADS 4
#define TABLE_SIZE (1024 * 1024)

// 全局配置
static ppdb_memtable_config_t g_memtable_config = {
    .type = PPDB_MEMTABLE_BASIC,  // 类型会在运行时根据测试类型设置
    .size_limit = TABLE_SIZE,
    .shard_count = 8,
    .sync = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 8,
        .backoff_us = 1,
        .enable_ref_count = false
    }
};

// 线程参数结构
typedef struct {
    ppdb_memtable_t* table;
    int thread_id;
    bool success;
} thread_args_t;

// 设置内存表配置
static void set_memtable_config(bool use_lockfree) {
    g_memtable_config.type = use_lockfree ? PPDB_MEMTABLE_LOCKFREE : PPDB_MEMTABLE_BASIC;
    g_memtable_config.sync.use_lockfree = use_lockfree;
}

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
            (const void*)key, strlen(key) + 1,
            (const void*)value, strlen(value) + 1);
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Put operation failed");
            args->success = false;
            return NULL;
        }

        // 读取并验证
        void* read_value = NULL;
        size_t value_size = 0;
        err = ppdb_memtable_get(args->table,
            (const void*)key, strlen(key) + 1,
            &read_value, &value_size);
        if (err != PPDB_OK) {
            PPDB_LOG_ERROR("Get operation failed");
            args->success = false;
            return NULL;
        }
        if (memcmp(read_value, value, strlen(value) + 1) != 0) {
            PPDB_LOG_ERROR("Value mismatch");
            args->success = false;
            free(read_value);
            return NULL;
        }
        free(read_value);

        // 随机删除一些键
        if (j % 3 == 0) {
            err = ppdb_memtable_delete(args->table,
                (const void*)key, strlen(key) + 1);
            if (err != PPDB_OK) {
                PPDB_LOG_ERROR("Delete operation failed");
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
    ppdb_error_t err = ppdb_memtable_create_with_config(&table, &g_memtable_config);
    PPDB_LOG_INFO("Create memtable result: %d", err);
    TEST_ASSERT(err == PPDB_OK, "Create memtable failed");
    TEST_ASSERT(table != NULL, "Memtable is NULL");

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    PPDB_LOG_INFO("Inserting key='%s' (len=%zu), value='%s' (len=%zu)",
                  test_key, strlen(test_key) + 1,
                  test_value, strlen(test_value) + 1);
    
    err = ppdb_memtable_put(table, (const void*)test_key, strlen(test_key) + 1,
                           (const void*)test_value, strlen(test_value) + 1);
    PPDB_LOG_INFO("Put operation result: %d", err);
    TEST_ASSERT(err == PPDB_OK, "Put operation failed");

    // 先获取值的大小
    size_t value_size = 0;
    PPDB_LOG_INFO("Getting value size for key='%s' (len=%zu)",
                  test_key, strlen(test_key) + 1);
    
    err = ppdb_memtable_get(table, (const void*)test_key, strlen(test_key) + 1,
                           NULL, &value_size);
    PPDB_LOG_INFO("Get size result: %d, value_size: %zu", err, value_size);
    TEST_ASSERT(err == PPDB_OK, "Get size failed");
    TEST_ASSERT(value_size == strlen(test_value) + 1, "Value size mismatch");

    // 获取值
    void* value_buf = NULL;
    size_t actual_size = 0;
    PPDB_LOG_INFO("Getting value for key='%s' (len=%zu)",
                  test_key, strlen(test_key) + 1);
    
    err = ppdb_memtable_get(table, (const void*)test_key, strlen(test_key) + 1,
                           &value_buf, &actual_size);
    PPDB_LOG_INFO("Get value result: %d, actual_size: %zu", err, actual_size);
    TEST_ASSERT(err == PPDB_OK, "Get value failed");
    TEST_ASSERT(actual_size == strlen(test_value) + 1, "Value size mismatch");
    TEST_ASSERT(memcmp(value_buf, test_value, actual_size) == 0, "Value content mismatch");
    free(value_buf);

    // 测试删除
    PPDB_LOG_INFO("Deleting key='%s' (len=%zu)",
                  test_key, strlen(test_key) + 1);
    
    err = ppdb_memtable_delete(table, (const void*)test_key, strlen(test_key) + 1);
    PPDB_LOG_INFO("Delete result: %d", err);
    TEST_ASSERT(err == PPDB_OK, "Delete operation failed");

    // 验证删除后无法获取
    PPDB_LOG_INFO("Verifying key is deleted");
    err = ppdb_memtable_get(table, (const void*)test_key, strlen(test_key) + 1,
                           NULL, &value_size);
    PPDB_LOG_INFO("Get after delete result: %d", err);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist after delete");

    ppdb_destroy(table);
    return 0;
}

// 分片测试
static int test_sharding(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create_with_config(&table, &g_memtable_config);
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

    ppdb_destroy(table);
    return 0;
}

// 并发操作测试
static int test_concurrent_ops(void) {
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create_with_config(&table, &g_memtable_config);
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

    ppdb_destroy(table);
    return 0;
}

int main(void) {
    TEST_INIT();
    const char* test_mode = getenv("PPDB_SYNC_MODE");
    bool use_lockfree = (test_mode && strcmp(test_mode, "lockfree") == 0);
    PPDB_LOG_INFO("Running Memtable Tests (%s mode)...", use_lockfree ? "lockfree" : "locked");
    
    // 设置同步配置
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
    
    // 创建内存表时传入配置
    g_memtable_config = (ppdb_memtable_config_t){
        .type = use_lockfree ? PPDB_MEMTABLE_LOCKFREE : PPDB_MEMTABLE_BASIC,
        .size_limit = 1024 * 1024,  // 1MB
        .shard_count = 16,
        .sync = config
    };
    
    RUN_TEST(test_basic_ops);
    RUN_TEST(test_sharding);
    RUN_TEST(test_concurrent_ops);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 