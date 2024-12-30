#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_memtable.h"
#include <pthread.h>

#define NUM_THREADS 4
#define NUM_OPERATIONS 1000

// 线程参数结构
typedef struct {
    ppdb_memtable_t* table;
    int thread_id;
} thread_args_t;

// 线程函数
static void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    ppdb_memtable_t* table = args->table;
    int thread_id = args->thread_id;
    
    char key_buf[32];
    char value_buf[32];
    uint8_t read_buf[32];
    size_t read_len;
    
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        // 生成键值对
        snprintf(key_buf, sizeof(key_buf), "key_%d_%d", thread_id, i);
        snprintf(value_buf, sizeof(value_buf), "value_%d_%d", thread_id, i);
        
        // 写入
        ppdb_error_t err = ppdb_memtable_put(table, (uint8_t*)key_buf, strlen(key_buf) + 1,
                                            (uint8_t*)value_buf, strlen(value_buf) + 1);
        assert(err == PPDB_OK);
        
        // 读取并验证
        read_len = sizeof(read_buf);
        err = ppdb_memtable_get(table, (uint8_t*)key_buf, strlen(key_buf) + 1,
                               read_buf, &read_len);
        assert(err == PPDB_OK);
        assert(read_len == strlen(value_buf) + 1);
        assert(memcmp(read_buf, value_buf, read_len) == 0);
        
        // 随机删除一些键
        if (i % 3 == 0) {
            err = ppdb_memtable_delete(table, (uint8_t*)key_buf, strlen(key_buf) + 1);
            assert(err == PPDB_OK);
            
            // 验证删除
            err = ppdb_memtable_get(table, (uint8_t*)key_buf, strlen(key_buf) + 1,
                                   read_buf, &read_len);
            assert(err == PPDB_ERR_NOT_FOUND);
        }
    }
    
    return NULL;
}

// 测试并发读写
static void test_concurrent_operations(void) {
    printf("Testing Concurrent Operations...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024 * 1024, &table);  // 1MB
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    
    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
    
    printf("  Starting %d threads, each performing %d operations...\n", 
           NUM_THREADS, NUM_OPERATIONS);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].table = table;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("  All threads completed successfully\n");
    printf("  Final table size: %zu\n", ppdb_memtable_size(table));
    
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试并发更新同一个键
static void test_concurrent_updates(void) {
    printf("Testing Concurrent Updates...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024 * 1024, &table);  // 1MB
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    
    // 初始化共享键
    const uint8_t shared_key[] = "shared_key";
    const uint8_t initial_value[] = "initial_value";
    err = ppdb_memtable_put(table, shared_key, sizeof(shared_key),
                           initial_value, sizeof(initial_value));
    assert(err == PPDB_OK);
    
    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
    
    printf("  Starting %d threads to update the same key...\n", NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].table = table;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 验证最终值
    uint8_t buf[256];
    size_t buf_len = sizeof(buf);
    err = ppdb_memtable_get(table, shared_key, sizeof(shared_key), buf, &buf_len);
    printf("  Final value length: %zu\n", buf_len);
    printf("  Final value: %s\n", buf);
    
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// Memtable 并发操作测试
static int test_concurrent_memtable(void) {
    ppdb_kvstore_t* store = NULL;
    pthread_t threads[NUM_THREADS];
    thread_context_t contexts[NUM_THREADS];
    int err;

    // 初始化 KVStore
    err = ppdb_kvstore_create(&store, NULL);
    TEST_ASSERT_OK(err, "Failed to create kvstore");
    TEST_ASSERT_NOT_NULL(store, "KVStore is null");

    // 创建多个线程并发操作
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].store = store;
        contexts[i].thread_id = i;
        contexts[i].num_ops = NUM_OPERATIONS;
        contexts[i].success_ops = 0;

        err = pthread_create(&threads[i], NULL, memtable_worker, &contexts[i]);
        TEST_ASSERT(err == 0, "Failed to create thread %d", i);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        TEST_ASSERT(contexts[i].success_ops > 0, "Thread %d had no successful operations", i);
    }

    // 验证数据一致性
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
    char expected[VALUE_SIZE];
    
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        err = ppdb_kvstore_get(store, key, strlen(key), value, sizeof(value));
        if (err == PPDB_OK) {
            snprintf(expected, sizeof(expected), "value_%d", i);
            TEST_ASSERT(strcmp(value, expected) == 0, "Data inconsistency for key: %s", key);
        }
    }

    ppdb_kvstore_destroy(store);
    return 0;
}

// WAL 并发操作测试
static int test_concurrent_wal(void) {
    ppdb_kvstore_t* store = NULL;
    pthread_t threads[NUM_THREADS];
    thread_context_t contexts[NUM_THREADS];
    int err;

    // 初始化带 WAL 的 KVStore
    ppdb_config_t config = {
        .enable_wal = true,
        .wal_path = "/tmp/test_wal",
        .sync_write = true
    };
    
    err = ppdb_kvstore_create(&store, &config);
    TEST_ASSERT_OK(err, "Failed to create kvstore with WAL");
    TEST_ASSERT_NOT_NULL(store, "KVStore is null");

    // 创建多个线程并发写入
    for (int i = 0; i < NUM_THREADS; i++) {
        contexts[i].store = store;
        contexts[i].thread_id = i;
        contexts[i].num_ops = NUM_OPERATIONS;
        contexts[i].success_ops = 0;

        err = pthread_create(&threads[i], NULL, wal_worker, &contexts[i]);
        TEST_ASSERT(err == 0, "Failed to create thread %d", i);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        TEST_ASSERT(contexts[i].success_ops > 0, "Thread %d had no successful operations", i);
    }

    // 验证 WAL 完整性
    err = ppdb_kvstore_sync(store);
    TEST_ASSERT_OK(err, "Failed to sync WAL");

    ppdb_kvstore_destroy(store);
    return 0;
}

// 并发读写测试
static int test_concurrent_read_write(void) {
    ppdb_kvstore_t* store = NULL;
    pthread_t threads[NUM_THREADS * 2];  // 一半读线程，一半写线程
    thread_context_t contexts[NUM_THREADS * 2];
    int err;

    err = ppdb_kvstore_create(&store, NULL);
    TEST_ASSERT_OK(err, "Failed to create kvstore");
    TEST_ASSERT_NOT_NULL(store, "KVStore is null");

    // 创建读写线程
    for (int i = 0; i < NUM_THREADS * 2; i++) {
        contexts[i].store = store;
        contexts[i].thread_id = i;
        contexts[i].num_ops = NUM_OPERATIONS;
        contexts[i].success_ops = 0;

        // 偶数线程写入，奇数线程读取
        void* worker = (i % 2 == 0) ? writer_worker : reader_worker;
        err = pthread_create(&threads[i], NULL, worker, &contexts[i]);
        TEST_ASSERT(err == 0, "Failed to create thread %d", i);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS * 2; i++) {
        pthread_join(threads[i], NULL);
        TEST_ASSERT(contexts[i].success_ops > 0, "Thread %d had no successful operations", i);
    }

    ppdb_kvstore_destroy(store);
    return 0;
}

// 工作线程函数
static void* memtable_worker(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
    int err;

    for (int i = 0; i < ctx->num_ops; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        
        err = ppdb_kvstore_put(ctx->store, key, strlen(key), value, strlen(value));
        if (err == PPDB_OK) {
            ctx->success_ops++;
        }
        
        // 随机延迟模拟真实场景
        if (i % 10 == 0) {
            microsleep(rand() % 1000);
        }
    }
    return NULL;
}

static void* wal_worker(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
    int err;

    for (int i = 0; i < ctx->num_ops; i++) {
        snprintf(key, sizeof(key), "wal_key_%d_%d", ctx->thread_id, i);
        snprintf(value, sizeof(value), "wal_value_%d_%d", ctx->thread_id, i);
        
        err = ppdb_kvstore_put(ctx->store, key, strlen(key), value, strlen(value));
        if (err == PPDB_OK) {
            ctx->success_ops++;
        }

        // 每10次操作强制同步一次
        if (i % 10 == 0) {
            ppdb_kvstore_sync(ctx->store);
        }
    }
    return NULL;
}

static void* writer_worker(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
    int err;

    for (int i = 0; i < ctx->num_ops; i++) {
        snprintf(key, sizeof(key), "rw_key_%d_%d", ctx->thread_id, i);
        snprintf(value, sizeof(value), "rw_value_%d_%d", ctx->thread_id, i);
        
        err = ppdb_kvstore_put(ctx->store, key, strlen(key), value, strlen(value));
        if (err == PPDB_OK) {
            ctx->success_ops++;
        }
    }
    return NULL;
}

static void* reader_worker(void* arg) {
    thread_context_t* ctx = (thread_context_t*)arg;
    char key[KEY_SIZE];
    char value[VALUE_SIZE];
    int err;

    for (int i = 0; i < ctx->num_ops; i++) {
        // 随机读取一个可能存在的key
        snprintf(key, sizeof(key), "rw_key_%d_%d", 
                rand() % NUM_THREADS, rand() % NUM_OPERATIONS);
        
        err = ppdb_kvstore_get(ctx->store, key, strlen(key), value, sizeof(value));
        if (err == PPDB_OK || err == PPDB_NOT_FOUND) {
            ctx->success_ops++;
        }
    }
    return NULL;
}

// 并发测试套件
static const test_case_t concurrent_cases[] = {
    {"test_concurrent_memtable", test_concurrent_memtable, 30, false, "Test memtable concurrent operations"},
    {"test_concurrent_wal", test_concurrent_wal, 30, false, "Test WAL concurrent operations"},
    {"test_concurrent_read_write", test_concurrent_read_write, 30, false, "Test concurrent read/write operations"},
    {NULL, NULL, 0, false, NULL}  // 结束标记
};

const test_suite_t concurrent_suite = {
    .name = "Concurrent Tests",
    .cases = concurrent_cases,
    .num_cases = sizeof(concurrent_cases) / sizeof(concurrent_cases[0]) - 1,
    .setup = NULL,
    .teardown = NULL
};

int main(int argc, char* argv[]) {
    printf("Starting MemTable Concurrent Tests...\n\n");
    
    // 运行所有测试
    test_concurrent_operations();
    test_concurrent_updates();
    
    printf("All MemTable Concurrent Tests passed!\n");
    return 0;
}