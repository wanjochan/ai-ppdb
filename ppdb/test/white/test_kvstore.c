#include <cosmopolitan.h>
#include "test_framework.h"
#include <ppdb/kvstore.h>
#include <ppdb/error.h>
#include <ppdb/logger.h>
#include <ppdb/fs.h>

// Forward declaration of worker thread function
static void* concurrent_worker(void* arg);

// Test KVStore create/close
static int test_kvstore_create_close(void) {
    ppdb_log_info("Testing KVStore create/close...");
    
    const char* test_dir = "test_kvstore_create.db";
    cleanup_test_dir(test_dir);
    
    // Create KVStore
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 4096,
        .l0_size = 4096 * 4,
        .l0_files = 4,
        .compression = PPDB_COMPRESSION_NONE
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    char err_msg[128];
    snprintf(err_msg, sizeof(err_msg), "Failed to create KVStore: %s", ppdb_error_string(err));
    TEST_ASSERT(err == PPDB_OK, err_msg);
    TEST_ASSERT(store != NULL, "KVStore pointer is NULL");
    
    // Close KVStore
    ppdb_kvstore_destroy(store);
    
    cleanup_test_dir(test_dir);
    return 0;
}

// Test KVStore basic operations
static int test_kvstore_basic_ops(void) {
    printf("Testing KVStore basic operations...\n");

    // 创建 KVStore
    ppdb_kvstore_t* store = create_test_kvstore("test_kvstore_basic.db", PPDB_MODE_LOCKED);
    assert(store != NULL);

    // 测试插入和获取
    const char* test_key = "test_key";
    const char* test_value = "test_value";
    ppdb_error_t err = ppdb_kvstore_put(store, (const uint8_t*)test_key, strlen(test_key),
                                       (const uint8_t*)test_value, strlen(test_value));
    assert(err == PPDB_OK);

    // 先获取值的大小
    size_t value_size = 0;
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key),
                          NULL, &value_size);
    assert(err == PPDB_OK);
    assert(value_size == strlen(test_value));

    // 分配足够的缓冲区并获取值
    uint8_t* value_buf = (uint8_t*)malloc(value_size + 1);
    assert(value_buf != NULL);
    size_t actual_size = value_size;
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key),
                          value_buf, &actual_size);
    assert(err == PPDB_OK);
    assert(actual_size == strlen(test_value));
    value_buf[actual_size] = '\0';  // 添加字符串结束符
    assert(strcmp((const char*)value_buf, test_value) == 0);
    free(value_buf);

    // 测试删除
    err = ppdb_kvstore_delete(store, (const uint8_t*)test_key, strlen(test_key));
    assert(err == PPDB_OK);

    // 验证删除后无法获取
    err = ppdb_kvstore_get(store, (const uint8_t*)test_key, strlen(test_key),
                          NULL, &value_size);
    assert(err == PPDB_ERR_NOT_FOUND);

    // 销毁 KVStore
    ppdb_kvstore_destroy(store);
}

// Test KVStore recovery
static int test_kvstore_recovery(void) {
    ppdb_log_info("Testing KVStore recovery...");
    
    const char* test_dir = "test_kvstore_recovery.db";
    cleanup_test_dir(test_dir);
    
    // First session: write data
    {
        ppdb_kvstore_config_t config = {
            .dir_path = {0},
            .memtable_size = 4096,
            .l0_size = 4096 * 4,
            .l0_files = 4,
            .compression = PPDB_COMPRESSION_NONE
        };
        strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
        
        ppdb_kvstore_t* store = NULL;
        ppdb_error_t err = ppdb_kvstore_create(&config, &store);
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Failed to create KVStore: %s", ppdb_error_string(err));
        TEST_ASSERT(err == PPDB_OK, err_msg);
        
        const uint8_t* key = (const uint8_t*)"recovery_key";
        const uint8_t* value = (const uint8_t*)"recovery_value";
        err = ppdb_kvstore_put(store, key, strlen((const char*)key), value, strlen((const char*)value));
        TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");
        
        ppdb_kvstore_close(store);
    }
    
    // Second session: recover data
    {
        ppdb_kvstore_config_t config = {
            .dir_path = {0},
            .memtable_size = 4096,
            .l0_size = 4096 * 4,
            .l0_files = 4,
            .compression = PPDB_COMPRESSION_NONE
        };
        strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
        
        ppdb_kvstore_t* store = NULL;
        ppdb_error_t err = ppdb_kvstore_create(&config, &store);
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Failed to create KVStore: %s", ppdb_error_string(err));
        TEST_ASSERT(err == PPDB_OK, err_msg);
        
        // Verify recovered data
        const uint8_t* key = (const uint8_t*)"recovery_key";
        uint8_t* buf = NULL;
        size_t size = 0;
        err = ppdb_kvstore_get(store, key, strlen((const char*)key), &buf, &size);
        TEST_ASSERT(err == PPDB_OK, "Failed to get value");
        TEST_ASSERT(strcmp((const char*)buf, "recovery_value") == 0, "Recovered value does not match");
        free(buf);  // 释放分配的内存
        
        ppdb_kvstore_destroy(store);
    }
    
    cleanup_test_dir(test_dir);
    return 0;
}

// Thread data structure
typedef struct {
    ppdb_kvstore_t* store;
    int thread_id;
    int success_count;
    pthread_mutex_t* mutex;  // 添加互斥锁用于计数
} thread_data_t;

// Worker thread function
static void* concurrent_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    ppdb_kvstore_t* store = data->store;
    int thread_id = data->thread_id;
    
    char key[32];
    char value[32];
    uint8_t* buf = NULL;
    size_t size;
    
    for (int i = 0; i < 100; i++) {
        // 每个线程使用独立的键空间
        memset(key, 0, sizeof(key));
        memset(value, 0, sizeof(value));
        snprintf(key, sizeof(key), "thread_%d_key_%d", thread_id, i);
        snprintf(value, sizeof(value), "thread_%d_value_%d", thread_id, i);
        
        // 重试写入逻辑
        int retries = 0;
        const int max_retries = 3;
        ppdb_error_t err;
        
        do {
            err = ppdb_kvstore_put(store, 
                                (const uint8_t*)key, strlen(key),
                                (const uint8_t*)value, strlen(value));
            if (err != PPDB_OK) {
                if (err == PPDB_ERR_FULL) {
                    // 如果 MemTable 已满，等待更长时间让 compaction 完成
                    usleep(100000);  // 100ms
                } else {
                    usleep(10000 * (retries + 1));  // 递增退避时间
                }
                ppdb_log_error("Thread %d put failed for key %s: %s (retry %d)",
                             thread_id, key, ppdb_error_string(err), retries);
            }
            retries++;
        } while (err != PPDB_OK && retries < max_retries);
        
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d gave up putting key %s after %d retries: %s",
                          thread_id, key, retries, ppdb_error_string(err));
            continue;
        }
        
        // 验证写入
        size = 0;
        err = ppdb_kvstore_get(store, (const uint8_t*)key, strlen(key), &buf, &size);
        if (err != PPDB_OK) {
            ppdb_log_error("Thread %d get failed for key %s: %s",
                          thread_id, key, ppdb_error_string(err));
            continue;
        }
        
        // 验证值是否匹配
        if (strcmp((const char*)buf, value) != 0) {
            ppdb_log_error("Thread %d value mismatch for key %s: expected=%s, got=%s",
                          thread_id, key, value, buf);
            free(buf);  // 释放分配的内存
            
            // 重试写入
            retries = 0;
            do {
                err = ppdb_kvstore_put(store, 
                                    (const uint8_t*)key, strlen(key),
                                    (const uint8_t*)value, strlen(value));
                if (err != PPDB_OK) {
                    if (err == PPDB_ERR_FULL) {
                        usleep(100000);  // 100ms
                    } else {
                        usleep(10000 * (retries + 1));
                    }
                    ppdb_log_error("Thread %d retry put failed for key %s: %s (retry %d)",
                                 thread_id, key, ppdb_error_string(err), retries);
                }
                retries++;
            } while (err != PPDB_OK && retries < max_retries);
            
            if (err == PPDB_OK) {
                // 再次验证
                size = 0;
                err = ppdb_kvstore_get(store, (const uint8_t*)key, strlen(key), &buf, &size);
                if (err == PPDB_OK && strcmp((const char*)buf, value) == 0) {
                    pthread_mutex_lock(data->mutex);
                    data->success_count++;
                    pthread_mutex_unlock(data->mutex);
                }
                free(buf);  // 释放分配的内存
            }
        } else {
            pthread_mutex_lock(data->mutex);
            data->success_count++;
            pthread_mutex_unlock(data->mutex);
            free(buf);  // 释放分配的内存
        }
    }
    
    return NULL;
}

// Test KVStore concurrent operations
static int test_kvstore_concurrent(void) {
    ppdb_log_info("Testing KVStore concurrent operations...");
    
    const char* test_dir = "test_kvstore_concurrent.db";
    cleanup_test_dir(test_dir);
    
    // 创建 KVStore，使用更大的配置
    ppdb_kvstore_config_t config = {
        .dir_path = {0},
        .memtable_size = 65536,  // 64KB
        .l0_size = 262144,      // 256KB
        .l0_files = 4,
        .compression = PPDB_COMPRESSION_NONE
    };
    strncpy(config.dir_path, test_dir, sizeof(config.dir_path) - 1);
    
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    char err_msg[128];
    snprintf(err_msg, sizeof(err_msg), "Failed to create KVStore: %s", ppdb_error_string(err));
    TEST_ASSERT(err == PPDB_OK, err_msg);
    
    // 创建线程
    const int num_threads = 4;
    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];
    pthread_mutex_t mutex;
    
    pthread_mutex_init(&mutex, NULL);
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].store = store;
        thread_data[i].thread_id = i;
        thread_data[i].success_count = 0;
        thread_data[i].mutex = &mutex;
        pthread_create(&threads[i], NULL, concurrent_worker, &thread_data[i]);
    }
    
    // 等待所有线程完成
    int total_success = 0;
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_success += thread_data[i].success_count;
    }
    
    pthread_mutex_destroy(&mutex);
    
    // 验证结果
    ppdb_log_info("Total successful operations: %d", total_success);
    TEST_ASSERT(total_success > 0, "No successful operations");
    
    // 验证所有键值对
    const int max_retries = 3;
    int retry_count = 0;
    bool all_verified = false;
    
    while (!all_verified && retry_count < max_retries) {
        all_verified = true;
        int verified_count = 0;
        
        for (int i = 0; i < num_threads; i++) {
            for (int j = 0; j < 100; j++) {
                char key[32];
                char expected_value[32];
                
                memset(key, 0, sizeof(key));
                memset(expected_value, 0, sizeof(expected_value));
                
                snprintf(key, sizeof(key), "thread_%d_key_%d", i, j);
                snprintf(expected_value, sizeof(expected_value), "thread_%d_value_%d", i, j);
                
                uint8_t* buf = NULL;
                size_t size = 0;
                err = ppdb_kvstore_get(store, (const uint8_t*)key, strlen(key), &buf, &size);
                if (err == PPDB_OK) {
                    if (strcmp((const char*)buf, expected_value) == 0) {
                        verified_count++;
                    } else {
                        ppdb_log_error("Value mismatch for key %s: expected=%s, got=%s",
                                     key, expected_value, buf);
                        all_verified = false;
                    }
                    free(buf);  // 释放分配的内存
                } else {
                    ppdb_log_error("Failed to get key %s: %s", key, ppdb_error_string(err));
                    all_verified = false;
                }
            }
        }
        
        if (!all_verified) {
            ppdb_log_info("Retry %d: Verified %d/%d keys",
                         retry_count + 1, verified_count, num_threads * 100);
            retry_count++;
            usleep(100000);  // 100ms
        }
    }
    
    TEST_ASSERT(all_verified, "Failed to verify all keys after retries");
    
    // 清理
    ppdb_kvstore_destroy(store);
    cleanup_test_dir(test_dir);
    return 0;
}

// KVStore test suite definition
static const test_case_t kvstore_test_cases[] = {
    {"create_close", test_kvstore_create_close},
    {"basic_ops", test_kvstore_basic_ops},
    {"recovery", test_kvstore_recovery},
    {"concurrent", test_kvstore_concurrent}
};

// Export KVStore test suite
const test_suite_t kvstore_suite = {
    .name = "KVStore",
    .cases = kvstore_test_cases,
    .num_cases = sizeof(kvstore_test_cases) / sizeof(kvstore_test_cases[0])
};