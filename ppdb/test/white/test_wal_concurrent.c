 #include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "test_framework.h"
#include "test_plan.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "src/kvstore/internal/kvstore_wal.h"
#include "src/kvstore/internal/kvstore_memtable.h"

#define NUM_THREADS 4
#define OPS_PER_THREAD 1000
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 128
#define TEST_DIR "./tmp_test_wal"

// 线程参数结构
typedef struct {
    ppdb_wal_t* wal;
    int thread_id;
    int num_ops;
    int success_ops;
} thread_args_t;

// WAL资源清理函数
static void cleanup_wal(void* ptr) {
    ppdb_wal_t* wal = (ppdb_wal_t*)ptr;
    if (wal) {
        ppdb_wal_close_lockfree(wal);
        ppdb_wal_destroy_lockfree(wal);
    }
}

// 生成测试数据
static void generate_test_data(char* key, char* value, int thread_id, int op_id) {
    snprintf(key, MAX_KEY_SIZE, "key_%d_%d", thread_id, op_id);
    snprintf(value, MAX_VALUE_SIZE, "value_%d_%d", thread_id, op_id);
}

// 并发写入线程函数
static void* concurrent_write_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    char key[MAX_KEY_SIZE];
    char value[MAX_VALUE_SIZE];
    
    for (int i = 0; i < args->num_ops; i++) {
        generate_test_data(key, value, args->thread_id, i);
        
        // 注入错误
        test_inject_error();
        
        ppdb_error_t err = ppdb_wal_write_lockfree(args->wal, 
            PPDB_WAL_RECORD_PUT,
            key, strlen(key), 
            value, strlen(value));
            
        if (err == PPDB_OK) {
            args->success_ops++;
        } else {
            ppdb_log_error("Thread %d failed to write op %d: %s", 
                args->thread_id, i, ppdb_error_string(err));
        }
    }
    
    return NULL;
}

// 验证WAL内容
static int verify_wal_contents(ppdb_wal_t* wal, thread_args_t* thread_args) {
    // 创建临时memtable用于恢复
    ppdb_memtable_t* table;
    ppdb_memtable_config_t config = {
        .size = 1024 * 1024 * 10,  // 10MB
        .dir = TEST_DIR
    };
    
    ppdb_error_t err = ppdb_memtable_create(&config, &table);
    TEST_ASSERT_OK(err, "Failed to create memtable");
    
    // 从WAL恢复到memtable
    err = ppdb_wal_recover_lockfree(wal, table);
    TEST_ASSERT_OK(err, "Failed to recover WAL");
    
    // 验证memtable中的数据
    int* thread_success = calloc(NUM_THREADS, sizeof(int));
    TEST_ASSERT_NOT_NULL(thread_success, "Failed to allocate thread_success array");
    
    ppdb_memtable_iter_t* iter = ppdb_memtable_iter_create(table);
    TEST_ASSERT_NOT_NULL(iter, "Failed to create memtable iterator");
    
    while (ppdb_memtable_iter_valid(iter)) {
        const void* key;
        size_t key_size;
        const void* value;
        size_t value_size;
        
        err = ppdb_memtable_iter_entry(iter, &key, &key_size, &value, &value_size);
        TEST_ASSERT_OK(err, "Failed to get memtable entry");
        
        // 解析key中的线程ID和操作ID
        int thread_id, op_id;
        sscanf((char*)key, "key_%d_%d", &thread_id, &op_id);
        
        // 验证value格式
        char expected_value[MAX_VALUE_SIZE];
        snprintf(expected_value, sizeof(expected_value), 
            "value_%d_%d", thread_id, op_id);
        
        TEST_ASSERT(thread_id >= 0 && thread_id < NUM_THREADS,
            "Invalid thread_id: %d", thread_id);
        TEST_ASSERT(value_size == strlen(expected_value),
            "Invalid value size");
        TEST_ASSERT(memcmp(value, expected_value, value_size) == 0,
            "Value mismatch");
        
        thread_success[thread_id]++;
        ppdb_memtable_iter_next(iter);
    }
    
    // 验证每个线程的成功操作数
    for (int i = 0; i < NUM_THREADS; i++) {
        TEST_ASSERT(thread_success[i] == thread_args[i].success_ops,
            "Thread %d success ops mismatch: expected %d, got %d",
            i, thread_args[i].success_ops, thread_success[i]);
    }
    
    free(thread_success);
    ppdb_memtable_iter_destroy(iter);
    ppdb_memtable_destroy(table);
    
    return 0;
}

// 测试并发写入
static int test_concurrent_write(void) {
    test_config_t config;
    test_get_config(&config);
    
    // 创建WAL
    ppdb_wal_t* wal;
    ppdb_wal_config_t wal_config = {
        .dir = TEST_DIR,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_wal_create_lockfree(&wal_config, &wal);
    TEST_ASSERT_OK(err, "Failed to create WAL");
    TEST_TRACK(wal, "wal", cleanup_wal);
    
    // 创建线程参数
    thread_args_t thread_args[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].wal = wal;
        thread_args[i].thread_id = i;
        thread_args[i].num_ops = OPS_PER_THREAD;
        thread_args[i].success_ops = 0;
        
        int ret = pthread_create(&threads[i], NULL, 
            concurrent_write_thread, &thread_args[i]);
        TEST_ASSERT(ret == 0, "Failed to create thread %d", i);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 验证WAL内容
    return verify_wal_contents(wal, thread_args);
}

// 测试并发写入和恢复
static int test_concurrent_write_recover(void) {
    // 创建WAL
    ppdb_wal_t* wal;
    ppdb_wal_config_t wal_config = {
        .dir = TEST_DIR,
        .sync_mode = PPDB_SYNC_MODE_ASYNC
    };
    
    ppdb_error_t err = ppdb_wal_create_lockfree(&wal_config, &wal);
    TEST_ASSERT_OK(err, "Failed to create WAL");
    TEST_TRACK(wal, "wal", cleanup_wal);
    
    // 设置错误注入
    error_injection_t error_config = {
        .enabled = true,
        .crash_probability = 0.001f,
        .delay_probability = 0.01f,
        .max_delay_ms = 100
    };
    test_set_error_injection(&error_config);
    
    // 创建线程参数
    thread_args_t thread_args[NUM_THREADS];
    pthread_t threads[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].wal = wal;
        thread_args[i].thread_id = i;
        thread_args[i].num_ops = OPS_PER_THREAD;
        thread_args[i].success_ops = 0;
        
        int ret = pthread_create(&threads[i], NULL, 
            concurrent_write_thread, &thread_args[i]);
        TEST_ASSERT(ret == 0, "Failed to create thread %d", i);
        
        // 每创建两个线程后尝试恢复
        if (i % 2 == 1) {
            ppdb_memtable_t* table;
            ppdb_memtable_config_t config = {
                .size = 1024 * 1024 * 10,
                .dir = TEST_DIR
            };
            
            err = ppdb_memtable_create(&config, &table);
            TEST_ASSERT_OK(err, "Failed to create memtable");
            
            err = ppdb_wal_recover_lockfree(wal, table);
            TEST_ASSERT_OK(err, "Failed to recover WAL");
            
            ppdb_memtable_destroy(table);
        }
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 验证WAL内容
    return verify_wal_contents(wal, thread_args);
}

// 注册WAL并发测试
void register_wal_concurrent_tests(void) {
    static const test_case_t cases[] = {
        {
            .name = "test_concurrent_write",
            .fn = test_concurrent_write,
            .timeout_seconds = 30,
            .skip = false,
            .description = "测试多线程并发写入WAL"
        },
        {
            .name = "test_concurrent_write_recover",
            .fn = test_concurrent_write_recover,
            .timeout_seconds = 30,
            .skip = false,
            .description = "测试多线程并发写入和恢复WAL"
        }
    };
    
    static const test_suite_t suite = {
        .name = "WAL Concurrent Tests",
        .cases = cases,
        .num_cases = sizeof(cases) / sizeof(cases[0]),
        .setup = NULL,
        .teardown = NULL
    };
    
    run_test_suite(&suite);
}