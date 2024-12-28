#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include "ppdb/logger.h"
#include "../../src_lockfree/kvstore/atomic_skiplist.h"

// 线程局部存储的随机数生成器状态
static __thread unsigned int rand_state = 0;

// 初始化线程局部随机数生成器
static void init_rand_state() {
    if (rand_state == 0) {
        rand_state = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
    }
}

// 线程安全的随机数生成
static int thread_safe_rand() {
    init_rand_state();
    return rand_r(&rand_state);
}

// 基本功能测试
static void test_basic_operations() {
    ppdb_log_info("Running basic operations test...");
    
    atomic_skiplist_t* list = NULL;
    ppdb_error_t err = atomic_skiplist_create(16, false, &list);
    assert(err == PPDB_OK);

    // 测试插入
    ppdb_slice_t key1 = {(uint8_t*)"key1", 4};
    ppdb_slice_t value1 = {(uint8_t*)"value1", 6};
    err = atomic_skiplist_insert(list, &key1, &value1);
    assert(err == PPDB_OK);

    // 测试查找
    ppdb_slice_t result = {NULL, 0};
    err = atomic_skiplist_find(list, &key1, &result);
    assert(err == PPDB_OK);
    assert(result.size == value1.size);
    assert(memcmp(result.data, value1.data, value1.size) == 0);
    free(result.data);

    // 测试删除
    err = atomic_skiplist_delete(list, &key1);
    assert(err == PPDB_OK);

    // 验证删除后查找
    err = atomic_skiplist_find(list, &key1, &result);
    assert(err == PPDB_ERR_NOT_FOUND);

    atomic_skiplist_destroy(list);
    ppdb_log_info("Basic operations test passed!");
}

// 并发测试参数
#define NUM_THREADS 4
#define OPS_PER_THREAD 1000
#define MAX_KEY_SIZE 64
#define MAX_VALUE_SIZE 128

// 操作类型
typedef enum {
    OP_INSERT = 0,
    OP_FIND = 1,
    OP_DELETE = 2,
    OP_COUNT
} op_type_t;

// 线程测试数据结构
typedef struct {
    atomic_skiplist_t* list;
    int thread_id;
    int num_ops;
} thread_data_t;

// 并发测试线程函数
static void* concurrent_test_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    char key_buf[MAX_KEY_SIZE];
    char value_buf[MAX_VALUE_SIZE];

    // 预先生成所有key
    for (int i = 0; i < data->num_ops; i++) {
        // 随机选择操作
        op_type_t op = thread_safe_rand() % OP_COUNT;
        
        // 生成key和value（确保不会溢出）
        snprintf(key_buf, sizeof(key_buf), "key_%d_%d", data->thread_id, i);
        snprintf(value_buf, sizeof(value_buf), "value_%d_%d", data->thread_id, i);
        
        ppdb_slice_t key = {(uint8_t*)key_buf, strlen(key_buf)};
        ppdb_slice_t value = {(uint8_t*)value_buf, strlen(value_buf)};

        switch (op) {
            case OP_INSERT: {
                ppdb_error_t err = atomic_skiplist_insert(data->list, &key, &value);
                assert(err == PPDB_OK || err == PPDB_ERR_INVALID);  // 可能已存在
                break;
            }
            case OP_FIND: {
                ppdb_slice_t result = {NULL, 0};
                ppdb_error_t err = atomic_skiplist_find(data->list, &key, &result);
                if (err == PPDB_OK) {
                    assert(result.size == value.size);
                    assert(memcmp(result.data, value.data, value.size) == 0);
                    free(result.data);
                }
                break;
            }
            case OP_DELETE: {
                ppdb_error_t err = atomic_skiplist_delete(data->list, &key);
                assert(err == PPDB_OK || err == PPDB_ERR_NOT_FOUND);  // 可能已被删除
                break;
            }
        }

        // 随机休眠一小段时间，增加并发机会
        if (thread_safe_rand() % 100 < 10) {  // 10%的概率
            struct timespec ts = {0, (thread_safe_rand() % 1000) * 1000};  // 0-1ms
            nanosleep(&ts, NULL);
        }
    }

    return NULL;
}

// 并发测试
static void test_concurrent_operations() {
    ppdb_log_info("Running concurrent operations test...");

    atomic_skiplist_t* list = NULL;
    ppdb_error_t err = atomic_skiplist_create(16, false, &list);
    assert(err == PPDB_OK);

    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].list = list;
        thread_data[i].thread_id = i;
        thread_data[i].num_ops = OPS_PER_THREAD;
        pthread_create(&threads[i], NULL, concurrent_test_thread, &thread_data[i]);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 获取统计信息
    ppdb_stats_t stats;
    atomic_skiplist_stats(list, &stats);
    ppdb_log_info("Final node count: %zu", stats.total_keys);

    atomic_skiplist_destroy(list);
    ppdb_log_info("Concurrent operations test passed!");
}

int main() {
    ppdb_log_info("Starting atomic skiplist tests...");

    // 运行基本功能测试
    test_basic_operations();

    // 运行并发测试
    test_concurrent_operations();

    ppdb_log_info("All tests passed!");
    return 0;
} 