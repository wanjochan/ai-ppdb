#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "test/white/test_framework.h"
#include "test/white/test_macros.h"

// 测试配置
#define TEST_NUM_THREADS 32
#define TEST_NUM_ITERATIONS 10000
#define TEST_MAX_KEY_SIZE 100
#define TEST_MAX_VALUE_SIZE 1000

// 测试函数声明
static void test_skiplist_basic(bool use_lockfree);
static void test_skiplist_concurrent(bool use_lockfree);
static void test_skiplist_iterator(bool use_lockfree);

int main(void) {
    // 从环境变量获取测试模式
    const char* test_mode = getenv("PPDB_SYNC_MODE");
    bool use_lockfree = (test_mode && strcmp(test_mode, "lockfree") == 0);
    
    printf("\n=== PPDB Skiplist Test Suite ===\n");
    printf("Test Mode: %s\n", use_lockfree ? "lockfree" : "locked");
    printf("Starting tests...\n\n");
    
    test_skiplist_basic(use_lockfree);
    test_skiplist_concurrent(use_lockfree);
    test_skiplist_iterator(use_lockfree);
    
    printf("\n=== All Tests Completed Successfully! ===\n");
    return 0;
}

// Thread-local storage keys
static pthread_key_t rand_state_key;
static pthread_once_t rand_key_once = PTHREAD_ONCE_INIT;

// Initialize the TLS key
static void create_rand_state_key(void) {
    pthread_key_create(&rand_state_key, free);
}

// Initialize thread local random number generator
static void init_rand_state(void) {
    uint32_t* state = pthread_getspecific(rand_state_key);
    if (state == NULL) {
        state = malloc(sizeof(uint32_t));
        *state = (uint32_t)time(NULL) ^ (uint32_t)pthread_self();
        pthread_setspecific(rand_state_key, state);
    }
}

// Thread-safe random number generation
static uint32_t thread_safe_rand(void) {
    pthread_once(&rand_key_once, create_rand_state_key);
    init_rand_state();
    
    uint32_t* state = pthread_getspecific(rand_state_key);
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// 基本操作测试
static void test_skiplist_basic(bool use_lockfree) {
    printf("Starting basic skiplist test (use_lockfree=%d)...\n", use_lockfree);
    
    ppdb_base_t* base = NULL;
    ppdb_error_t err;
    
    // 创建skiplist
    err = ppdb_create(PPDB_TYPE_SKIPLIST, &base);
    printf("Create skiplist result: %d\n", err);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");
    TEST_ASSERT(base != NULL, "Base pointer is NULL");

    // 测试插入
    uint8_t* key_data = PPDB_ALIGNED_ALLOC(4);
    memcpy(key_data, "key1", 4);
    uint8_t* value_data = PPDB_ALIGNED_ALLOC(6);
    memcpy(value_data, "value1", 6);
    
    ppdb_key_t key1 = {
        .data = key_data,
        .size = 4,
        .ref_count = {.value = 1}
    };
    ppdb_value_t value1 = {
        .data = value_data,
        .size = 6,
        .ref_count = {.value = 1}
    };
    
    printf("Putting key-value pair...\n");
    err = ppdb_put(base, &key1, &value1);
    printf("Put result: %d\n", err);
    TEST_ASSERT(err == PPDB_OK, "Failed to put key-value pair");

    // 测试查找
    printf("Getting value...\n");
    ppdb_value_t result = {0};
    err = ppdb_get(base, &key1, &result);
    printf("Get result: %d\n", err);
    TEST_ASSERT(err == PPDB_OK, "Failed to get value");
    
    printf("Comparing values...\n");
    printf("Expected size: %zu, Actual size: %zu\n", value1.size, result.size);
    TEST_ASSERT(result.size == value1.size, "Value size mismatch");
    ASSERT_MEM_EQ(result.data, value1.data, value1.size);
    PPDB_ALIGNED_FREE(result.data);

    // 测试删除
    printf("Removing key...\n");
    err = ppdb_remove(base, &key1);
    printf("Remove result: %d\n", err);
    TEST_ASSERT(err == PPDB_OK, "Failed to remove key");

    // 验证删除后查找
    printf("Verifying removal...\n");
    err = ppdb_get(base, &key1, &result);
    printf("Get after remove result: %d\n", err);
    TEST_ASSERT(err == PPDB_ERR_NOT_FOUND, "Key should not exist after removal");

    // 清理内存
    PPDB_ALIGNED_FREE(key_data);
    PPDB_ALIGNED_FREE(value_data);
    
    printf("Destroying skiplist...\n");
    ppdb_destroy(base);
    printf("Basic test completed\n");
}

// 并发测试参数
#define CONCURRENT_NUM_THREADS 4
#define CONCURRENT_OPS_PER_THREAD 1000
#define CONCURRENT_MAX_KEY_SIZE 64
#define CONCURRENT_MAX_VALUE_SIZE 128

// 操作类型
typedef enum {
    OP_INSERT = 0,
    OP_FIND = 1,
    OP_DELETE = 2,
    OP_COUNT
} op_type_t;

// 线程测试数据结构
typedef struct {
    ppdb_base_t* base;
    int thread_id;
    int num_ops;
} thread_data_t;

// 并发测试线程函数
static void* concurrent_test_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    uint8_t key_buf[CONCURRENT_MAX_KEY_SIZE];
    uint8_t value_buf[CONCURRENT_MAX_VALUE_SIZE];

    for (int i = 0; i < data->num_ops; i++) {
        // 随机选择操作
        op_type_t op = thread_safe_rand() % OP_COUNT;
        
        // 生成key和value
        int key_size = snprintf((char*)key_buf, sizeof(key_buf), "key_%d_%d", data->thread_id, i);
        int value_size = snprintf((char*)value_buf, sizeof(value_buf), "value_%d_%d", data->thread_id, i);
        
        ppdb_key_t key = {
            .data = key_buf,
            .size = key_size,
            .ref_count = {.value = 1}
        };
        
        ppdb_value_t value = {
            .data = value_buf,
            .size = value_size,
            .ref_count = {.value = 1}
        };

        switch (op) {
            case OP_INSERT: {
                ppdb_error_t err = ppdb_put(data->base, &key, &value);
                TEST_ASSERT(err == PPDB_OK || err == PPDB_ERR_ALREADY_EXISTS, 
                          "Insert operation failed unexpectedly");
                break;
            }
            case OP_FIND: {
                ppdb_value_t result = {0};
                ppdb_error_t err = ppdb_get(data->base, &key, &result);
                if (err == PPDB_OK) {
                    TEST_ASSERT(result.size == value.size, "Value size mismatch");
                    ASSERT_MEM_EQ(result.data, value.data, value.size);
                    PPDB_ALIGNED_FREE(result.data);
                }
                break;
            }
            case OP_DELETE: {
                ppdb_error_t err = ppdb_remove(data->base, &key);
                TEST_ASSERT(err == PPDB_OK || err == PPDB_ERR_NOT_FOUND,
                          "Delete operation failed unexpectedly");
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
static void test_skiplist_concurrent(bool use_lockfree) {
    ppdb_base_t* base = NULL;
    ppdb_error_t err;
    
    // 创建skiplist
    err = ppdb_create(PPDB_TYPE_SKIPLIST, &base);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");
    TEST_ASSERT(base != NULL, "Base pointer is NULL");

    pthread_t threads[CONCURRENT_NUM_THREADS];
    thread_data_t thread_data[CONCURRENT_NUM_THREADS];

    // 创建线程
    for (int i = 0; i < CONCURRENT_NUM_THREADS; i++) {
        thread_data[i].base = base;
        thread_data[i].thread_id = i;
        thread_data[i].num_ops = CONCURRENT_OPS_PER_THREAD;
        int ret = pthread_create(&threads[i], NULL, concurrent_test_thread, &thread_data[i]);
        TEST_ASSERT(ret == 0, "Failed to create thread");
    }

    // 等待所有线程完成
    for (int i = 0; i < CONCURRENT_NUM_THREADS; i++) {
        int ret = pthread_join(threads[i], NULL);
        TEST_ASSERT(ret == 0, "Failed to join thread");
    }

    ppdb_destroy(base);
}

// 迭代器测试
static void test_skiplist_iterator(bool use_lockfree) {
    ppdb_base_t* base = NULL;
    ppdb_error_t err;
    
    // 创建skiplist
    err = ppdb_create(PPDB_TYPE_SKIPLIST, &base);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");
    TEST_ASSERT(base != NULL, "Base pointer is NULL");

    // 插入测试数据
    const int NUM_ITEMS = 100;
    for (int i = 0; i < NUM_ITEMS; i++) {
        char key_buf[32], value_buf[32];
        snprintf(key_buf, sizeof(key_buf), "key_%03d", i);
        snprintf(value_buf, sizeof(value_buf), "value_%03d", i);
        
        ppdb_key_t key = {
            .data = (uint8_t*)key_buf,
            .size = strlen(key_buf),
            .ref_count = {.value = 1}
        };
        ppdb_value_t value = {
            .data = (uint8_t*)value_buf,
            .size = strlen(value_buf),
            .ref_count = {.value = 1}
        };
        
        err = ppdb_put(base, &key, &value);
        TEST_ASSERT(err == PPDB_OK, "Failed to insert test data");
    }

    // TODO: 实现迭代器遍历测试
    // 当迭代器接口实现后，添加相应的测试代码

    ppdb_destroy(base);
}
