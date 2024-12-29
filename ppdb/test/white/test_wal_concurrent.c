 #include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "test_framework.h"
#include "test_plan.h"
#include "ppdb/wal.h"
#include "ppdb/logger.h"

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
        
        ppdb_error_t err = ppdb_wal_append(args->wal, 
            (uint8_t*)key, strlen(key), 
            (uint8_t*)value, strlen(value));
            
        if (err == PPDB_OK) {
            args->success_ops++;
        } else {
            ppdb_log_error("Thread %d failed to write op %d: %d", 
                args->thread_id, i, err);
        }
    }
    
    return NULL;
}

// 验证写入结果
static void verify_wal_contents(ppdb_wal_t* wal, thread_args_t* thread_args) {
    char key[MAX_KEY_SIZE];
    char value[MAX_VALUE_SIZE];
    char read_value[MAX_VALUE_SIZE];
    size_t read_size;
    int total_verified = 0;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < thread_args[t].num_ops; i++) {
            generate_test_data(key, value, t, i);
            
            ppdb_error_t err = ppdb_wal_read(wal, 
                (uint8_t*)key, strlen(key),
                (uint8_t*)read_value, sizeof(read_value),
                &read_size);
                
            if (err == PPDB_OK) {
                assert(read_size == strlen(value));
                assert(memcmp(value, read_value, read_size) == 0);
                total_verified++;
            }
        }
    }
    
    ppdb_log_info("Verified %d entries in WAL", total_verified);
}

// 测试并发写入
void test_concurrent_write(void) {
    ppdb_log_info("Running WAL concurrent write test...");
    
    // 创建WAL
    ppdb_wal_t* wal = NULL;
    ppdb_error_t err = ppdb_wal_create(TEST_DIR, &wal);
    assert(err == PPDB_OK);
    
    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_args_t thread_args[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].wal = wal;
        thread_args[i].thread_id = i;
        thread_args[i].num_ops = OPS_PER_THREAD;
        thread_args[i].success_ops = 0;
        
        pthread_create(&threads[i], NULL, 
            concurrent_write_thread, &thread_args[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        ppdb_log_info("Thread %d completed with %d successful ops", 
            i, thread_args[i].success_ops);
    }
    
    // 验证写入结果
    verify_wal_contents(wal, thread_args);
    
    // 清理
    ppdb_wal_close(wal);
    ppdb_log_info("WAL concurrent write test completed");
}

// 测试并发写入和归档
void test_concurrent_write_archive(void) {
    ppdb_log_info("Running WAL concurrent write with archive test...");
    
    // 创建WAL（启用自动归档）
    ppdb_wal_t* wal = NULL;
    ppdb_wal_options_t options = {
        .auto_archive = true,
        .archive_size_threshold = 1024 * 1024  // 1MB
    };
    
    ppdb_error_t err = ppdb_wal_create_with_options(TEST_DIR, &options, &wal);
    assert(err == PPDB_OK);
    
    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_args_t thread_args[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_args[i].wal = wal;
        thread_args[i].thread_id = i;
        thread_args[i].num_ops = OPS_PER_THREAD * 2;  // 写入更多数据触发归档
        thread_args[i].success_ops = 0;
        
        pthread_create(&threads[i], NULL, 
            concurrent_write_thread, &thread_args[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        ppdb_log_info("Thread %d completed with %d successful ops", 
            i, thread_args[i].success_ops);
    }
    
    // 验证写入结果
    verify_wal_contents(wal, thread_args);
    
    // 检查归档状态
    ppdb_wal_stats_t stats;
    err = ppdb_wal_get_stats(wal, &stats);
    assert(err == PPDB_OK);
    ppdb_log_info("WAL stats: current_size=%zu, archived_size=%zu, num_archives=%d",
        stats.current_size, stats.archived_size, stats.num_archives);
    
    // 清理
    ppdb_wal_close(wal);
    ppdb_log_info("WAL concurrent write with archive test completed");
}

// 注册所有WAL并发测试
void register_wal_concurrent_tests(void) {
    TEST_REGISTER(test_concurrent_write);
    TEST_REGISTER(test_concurrent_write_archive);
}