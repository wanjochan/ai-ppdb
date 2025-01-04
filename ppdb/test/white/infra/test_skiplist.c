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
    // 初始化日志系统
    ppdb_log_init(&(ppdb_log_config_t){
        .enabled = true,
        .level = PPDB_LOG_DEBUG,
        .log_file = NULL,
        .outputs = 1  // stdout
    });
    
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
    
    ppdb_log_cleanup();
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
    ppdb_config_t config = {
        .type = PPDB_TYPE_SKIPLIST,
        .path = NULL,
        .memtable_size = DEFAULT_MEMTABLE_SIZE,
        .shard_count = DEFAULT_SHARD_COUNT,
        .use_lockfree = use_lockfree
    };
    err = ppdb_create(&base, &config);
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
    size_t op_counts[OP_COUNT];
    size_t op_success[OP_COUNT];
} thread_data_t;

// 并发测试线程函数
static void* concurrent_test_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    // 使用对齐分配
    uint8_t* key_buf = PPDB_ALIGNED_ALLOC(CONCURRENT_MAX_KEY_SIZE);
    uint8_t* value_buf = PPDB_ALIGNED_ALLOC(CONCURRENT_MAX_VALUE_SIZE);
    if (!key_buf || !value_buf) {
        if (key_buf) PPDB_ALIGNED_FREE(key_buf);
        if (value_buf) PPDB_ALIGNED_FREE(value_buf);
        return NULL;
    }

    // 记录操作统计
    size_t op_counts[OP_COUNT] = {0};
    size_t op_success[OP_COUNT] = {0};

    for (int i = 0; i < data->num_ops; i++) {
        // 随机选择操作
        op_type_t op = thread_safe_rand() % OP_COUNT;
        op_counts[op]++;
        
        // 生成key和value
        int key_size = snprintf((char*)key_buf, CONCURRENT_MAX_KEY_SIZE, 
                              "key_%d_%d", data->thread_id, i);
        int value_size = snprintf((char*)value_buf, CONCURRENT_MAX_VALUE_SIZE, 
                                "value_%d_%d", data->thread_id, i);
        
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
                if (err == PPDB_OK || err == PPDB_ERR_ALREADY_EXISTS) {
                    op_success[op]++;
                }
                TEST_ASSERT(err == PPDB_OK || err == PPDB_ERR_ALREADY_EXISTS, 
                          "Insert operation failed unexpectedly");
                break;
            }
            case OP_FIND: {
                ppdb_value_t result = {0};
                ppdb_error_t err = ppdb_get(data->base, &key, &result);
                if (err == PPDB_OK) {
                    op_success[op]++;
                    TEST_ASSERT(result.size == value.size, "Value size mismatch");
                    ASSERT_MEM_EQ(result.data, value.data, value.size);
                    PPDB_ALIGNED_FREE(result.data);
                }
                break;
            }
            case OP_DELETE: {
                ppdb_error_t err = ppdb_remove(data->base, &key);
                if (err == PPDB_OK || err == PPDB_ERR_NOT_FOUND) {
                    op_success[op]++;
                }
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

    // 清理内存
    PPDB_ALIGNED_FREE(key_buf);
    PPDB_ALIGNED_FREE(value_buf);

    // 保存统计结果
    memcpy(data->op_counts, op_counts, sizeof(op_counts));
    memcpy(data->op_success, op_success, sizeof(op_success));
    
    return NULL;
}

// 并发操作测试
static void test_skiplist_concurrent(bool use_lockfree) {
    printf("Starting concurrent skiplist test (use_lockfree=%d)...\n", use_lockfree);
    
    ppdb_base_t* base = NULL;
    ppdb_error_t err;
    
    // 创建skiplist
    ppdb_config_t config = {
        .type = PPDB_TYPE_SKIPLIST,
        .path = NULL,
        .memtable_size = DEFAULT_MEMTABLE_SIZE,
        .shard_count = DEFAULT_SHARD_COUNT,
        .use_lockfree = use_lockfree
    };
    err = ppdb_create(&base, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");
    TEST_ASSERT(base != NULL, "Base pointer is NULL");

    pthread_t threads[CONCURRENT_NUM_THREADS];
    thread_data_t thread_data[CONCURRENT_NUM_THREADS];

    // 创建线程
    for (int i = 0; i < CONCURRENT_NUM_THREADS; i++) {
        thread_data[i].base = base;
        thread_data[i].thread_id = i;
        thread_data[i].num_ops = CONCURRENT_OPS_PER_THREAD;
        memset(thread_data[i].op_counts, 0, sizeof(thread_data[i].op_counts));
        memset(thread_data[i].op_success, 0, sizeof(thread_data[i].op_success));
        
        int ret = pthread_create(&threads[i], NULL, concurrent_test_thread, &thread_data[i]);
        TEST_ASSERT(ret == 0, "Failed to create thread");
    }

    // 等待所有线程完成
    for (int i = 0; i < CONCURRENT_NUM_THREADS; i++) {
        int ret = pthread_join(threads[i], NULL);
        TEST_ASSERT(ret == 0, "Failed to join thread");
    }

    // 验证统计信息
    ppdb_metrics_t metrics;
    err = ppdb_storage_get_stats(base, &metrics);
    TEST_ASSERT(err == PPDB_OK, "Failed to get storage stats");

    // 汇总所有线程的操作统计
    size_t total_ops[OP_COUNT] = {0};
    size_t total_success[OP_COUNT] = {0};
    for (int i = 0; i < CONCURRENT_NUM_THREADS; i++) {
        for (int j = 0; j < OP_COUNT; j++) {
            total_ops[j] += thread_data[i].op_counts[j];
            total_success[j] += thread_data[i].op_success[j];
        }
    }

    // 打印统计信息
    printf("Concurrent test results:\n");
    printf("Total operations: %zu\n", total_ops[0] + total_ops[1] + total_ops[2]);
    printf("Insert ops: %zu (success: %zu)\n", total_ops[OP_INSERT], total_success[OP_INSERT]);
    printf("Find ops: %zu (success: %zu)\n", total_ops[OP_FIND], total_success[OP_FIND]);
    printf("Delete ops: %zu (success: %zu)\n", total_ops[OP_DELETE], total_success[OP_DELETE]);
    printf("Storage metrics:\n");
    printf("Get count: %zu (hits: %zu)\n", 
           ppdb_sync_counter_load(&metrics.get_count),
           ppdb_sync_counter_load(&metrics.get_hits));
    printf("Put count: %zu\n", ppdb_sync_counter_load(&metrics.put_count));
    printf("Remove count: %zu\n", ppdb_sync_counter_load(&metrics.remove_count));

    ppdb_destroy(base);
    printf("Concurrent test completed\n");
}

// 迭代器测试
static void test_skiplist_iterator(bool use_lockfree) {
    printf("Starting iterator test (use_lockfree=%d)...\n", use_lockfree);
    
    ppdb_base_t* base = NULL;
    ppdb_error_t err;
    
    // 创建skiplist
    ppdb_config_t config = {
        .type = PPDB_TYPE_SKIPLIST,
        .path = NULL,
        .memtable_size = DEFAULT_MEMTABLE_SIZE,
        .shard_count = DEFAULT_SHARD_COUNT,
        .use_lockfree = use_lockfree
    };
    err = ppdb_create(&base, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to create skiplist");
    TEST_ASSERT(base != NULL, "Base pointer is NULL");

    // 插入一些测试数据
    for (int i = 0; i < 10; i++) {
        char key_buf[32], value_buf[32];
        snprintf(key_buf, sizeof(key_buf), "key_%d", i);
        snprintf(value_buf, sizeof(value_buf), "value_%d", i);
        
        ppdb_key_t key = {
            .data = key_buf,
            .size = strlen(key_buf),
            .ref_count = {.value = 1}
        };
        ppdb_value_t value = {
            .data = value_buf,
            .size = strlen(value_buf),
            .ref_count = {.value = 1}
        };
        
        err = ppdb_put(base, &key, &value);
        TEST_ASSERT(err == PPDB_OK, "Failed to put test data");
    }

    // 初始化迭代器
    void* iter = NULL;
    err = ppdb_iterator_init(base, &iter);
    TEST_ASSERT(err == PPDB_OK, "Failed to initialize iterator");
    TEST_ASSERT(iter != NULL, "Iterator is NULL");

    // 遍历所有数据
    int count = 0;
    ppdb_key_t key;
    ppdb_value_t value;
    while ((err = ppdb_iterator_next(iter, &key, &value)) == PPDB_OK) {
        printf("Iter %d: key=%.*s, value=%.*s\n", 
               count, (int)key.size, (char*)key.data,
               (int)value.size, (char*)value.data);
        
        PPDB_ALIGNED_FREE(key.data);
        PPDB_ALIGNED_FREE(value.data);
        count++;
    }
    TEST_ASSERT(err == PPDB_ERR_ITERATOR_END, "Iterator should end with PPDB_ERR_ITERATOR_END");
    TEST_ASSERT(count == 10, "Iterator should return all 10 items");

    // 销毁迭代器
    ppdb_iterator_destroy(iter);

    // 销毁skiplist
    ppdb_destroy(base);
    printf("Iterator test completed\n");
}
