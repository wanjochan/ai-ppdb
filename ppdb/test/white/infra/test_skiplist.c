#include <cosmopolitan.h>
#include "test_framework.h"
#include "kvstore/internal/skiplist.h"

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

// 基本操作测试
void test_basic_ops(void) {
    ppdb_skiplist_t* list = NULL;
    ppdb_error_t err = ppdb_skiplist_create(16, false, &list);
    ASSERT_EQ(err, PPDB_OK);

    // 测试插入
    ppdb_slice_t key1 = {(uint8_t*)"key1", 4};
    ppdb_slice_t value1 = {(uint8_t*)"value1", 6};
    err = ppdb_skiplist_insert(list, &key1, &value1);
    ASSERT_EQ(err, PPDB_OK);

    // 测试查找
    ppdb_slice_t result = {NULL, 0};
    err = ppdb_skiplist_find(list, &key1, &result);
    ASSERT_EQ(err, PPDB_OK);
    ASSERT_EQ(result.size, value1.size);
    ASSERT_MEM_EQ(result.data, value1.data, value1.size);
    free(result.data);

    // 测试删除
    err = ppdb_skiplist_delete(list, &key1);
    ASSERT_EQ(err, PPDB_OK);

    // 验证删除后查找
    err = ppdb_skiplist_find(list, &key1, &result);
    ASSERT_EQ(err, PPDB_ERR_NOT_FOUND);

    ppdb_skiplist_destroy(list);
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
    ppdb_skiplist_t* list;
    int thread_id;
    int num_ops;
} thread_data_t;

// 并发测试线程函数
static void* concurrent_test_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    char key_buf[MAX_KEY_SIZE];
    char value_buf[MAX_VALUE_SIZE];

    for (int i = 0; i < data->num_ops; i++) {
        // 随机选择操作
        op_type_t op = thread_safe_rand() % OP_COUNT;
        
        // 生成key和value
        snprintf(key_buf, sizeof(key_buf), "key_%d_%d", data->thread_id, i);
        snprintf(value_buf, sizeof(value_buf), "value_%d_%d", data->thread_id, i);
        
        ppdb_slice_t key = {(uint8_t*)key_buf, strlen(key_buf)};
        ppdb_slice_t value = {(uint8_t*)value_buf, strlen(value_buf)};

        switch (op) {
            case OP_INSERT: {
                ppdb_error_t err = ppdb_skiplist_insert(data->list, &key, &value);
                assert(err == PPDB_OK || err == PPDB_ERR_INVALID);  // 可能已存在
                break;
            }
            case OP_FIND: {
                ppdb_slice_t result = {NULL, 0};
                ppdb_error_t err = ppdb_skiplist_find(data->list, &key, &result);
                if (err == PPDB_OK) {
                    assert(result.size == value.size);
                    assert(memcmp(result.data, value.data, value.size) == 0);
                    free(result.data);
                }
                break;
            }
            case OP_DELETE: {
                ppdb_error_t err = ppdb_skiplist_delete(data->list, &key);
                assert(err == PPDB_OK || err == PPDB_ERR_NOT_FOUND);  // 可能已被删除
                break;
            }
        }

        // 随机休眠，增加并发机会
        if (thread_safe_rand() % 100 < 10) {  // 10%的概率
            struct timespec ts = {0, (thread_safe_rand() % 1000) * 1000};  // 0-1ms
            nanosleep(&ts, NULL);
        }
    }

    return NULL;
}

// 并发操作测试
void test_concurrent_ops(void) {
    ppdb_skiplist_t* list = NULL;
    ppdb_error_t err = ppdb_skiplist_create(16, false, &list);
    ASSERT_EQ(err, PPDB_OK);

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
    ppdb_skiplist_stats(list, &stats);

    ppdb_skiplist_destroy(list);
}

// 迭代器测试
void test_iterator(void) {
    ppdb_skiplist_t* list = NULL;
    ppdb_error_t err = ppdb_skiplist_create(16, false, &list);
    ASSERT_EQ(err, PPDB_OK);

    // 插入有序数据
    for (int i = 0; i < 100; i++) {
        char key_buf[16], value_buf[16];
        snprintf(key_buf, sizeof(key_buf), "key_%03d", i);
        snprintf(value_buf, sizeof(value_buf), "val_%03d", i);
        
        ppdb_slice_t key = {(uint8_t*)key_buf, strlen(key_buf)};
        ppdb_slice_t value = {(uint8_t*)value_buf, strlen(value_buf)};
        
        err = ppdb_skiplist_insert(list, &key, &value);
        ASSERT_EQ(err, PPDB_OK);
    }

    // 测试正向遍历
    ppdb_iterator_t* it = NULL;
    err = ppdb_skiplist_iterator_create(list, &it);
    ASSERT_EQ(err, PPDB_OK);

    int count = 0;
    while (ppdb_iterator_valid(it)) {
        ppdb_slice_t key = ppdb_iterator_key(it);
        ppdb_slice_t value = ppdb_iterator_value(it);
        
        char expected_key[16], expected_value[16];
        snprintf(expected_key, sizeof(expected_key), "key_%03d", count);
        snprintf(expected_value, sizeof(expected_value), "val_%03d", count);
        
        ASSERT_EQ(key.size, strlen(expected_key));
        ASSERT_MEM_EQ(key.data, expected_key, key.size);
        ASSERT_EQ(value.size, strlen(expected_value));
        ASSERT_MEM_EQ(value.data, expected_value, value.size);
        
        count++;
        ppdb_iterator_next(it);
    }
    ASSERT_EQ(count, 100);

    ppdb_iterator_destroy(it);
    ppdb_skiplist_destroy(list);
}

int main(void) {
    TEST_INIT("Lock-free Skiplist Test");
    
    RUN_TEST(test_basic_ops);
    RUN_TEST(test_concurrent_ops);
    RUN_TEST(test_iterator);
    
    TEST_SUMMARY();
    return TEST_RESULT();
} 