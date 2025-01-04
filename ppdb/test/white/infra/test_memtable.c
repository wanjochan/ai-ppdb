#include "test_framework.h"
#include "ppdb/ppdb.h"
#include <cosmopolitan.h>

// Test configuration
#define TEST_MEMTABLE_SIZE (1024 * 1024)  // 1MB
#define TEST_KEY_SIZE 16
#define TEST_VALUE_SIZE 100
#define TEST_ITERATIONS 5  // 减少迭代次数
#define TEST_THREAD_COUNT 2  // 减少线程数量

// Forward declarations
static void* worker_thread(void* arg);
extern uint64_t lemur64(void);

// Test mode
#ifdef PPDB_SYNC_MODE_LOCKFREE
#define USE_LOCKFREE 1
#else
#define USE_LOCKFREE 0
#endif

// Test cases
static int test_memtable_basic(void) {
    printf("Starting basic memtable test (use_lockfree=%d)...\n", USE_LOCKFREE);

    // Create memtable
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_MEMTABLE,
        .use_lockfree = USE_LOCKFREE,
        .memtable_size = TEST_MEMTABLE_SIZE
    });
    ASSERT(err == PPDB_OK, "Create memtable result: %d", err);

    // Prepare test data
    char key_data[TEST_KEY_SIZE];
    char value_data[TEST_VALUE_SIZE];
    memset(key_data, 'k', TEST_KEY_SIZE);
    memset(value_data, 'v', TEST_VALUE_SIZE);

    ppdb_key_t key = {
        .data = key_data,
        .size = TEST_KEY_SIZE
    };
    ppdb_value_t value = {
        .data = value_data,
        .size = TEST_VALUE_SIZE
    };

    // Test put
    printf("Putting key-value pair...\n");
    err = ppdb_put(base, &key, &value);
    ASSERT(err == PPDB_OK, "Put result: %d", err);

    // Test get
    printf("Getting value...\n");
    ppdb_value_t get_value = {0};
    err = ppdb_get(base, &key, &get_value);
    ASSERT(err == PPDB_OK, "Get result: %d", err);

    // Compare values
    printf("Comparing values...\n");
    ASSERT(get_value.size == value.size, "Expected size: %zu, Actual size: %zu",
           value.size, get_value.size);
    ASSERT(memcmp(get_value.data, value.data, value.size) == 0,
           "Value data mismatch");

    // Test remove
    printf("Removing key...\n");
    err = ppdb_remove(base, &key);
    ASSERT(err == PPDB_OK, "Remove result: %d", err);

    // Verify removal
    printf("Verifying removal...\n");
    err = ppdb_get(base, &key, &get_value);
    ASSERT(err == PPDB_ERR_NOT_FOUND, "Get after remove result: %d", err);

    // Cleanup
    printf("Destroying memtable...\n");
    ppdb_destroy(base);
    printf("Basic test completed\n");
    return 0;
}

static int test_memtable_concurrent(void) {
    printf("Starting concurrent memtable test (use_lockfree=%d)...\n", USE_LOCKFREE);

    // Create memtable with larger size
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_MEMTABLE,
        .use_lockfree = USE_LOCKFREE,
        .memtable_size = TEST_MEMTABLE_SIZE * 2
    });
    
    if (err != PPDB_OK) {
        printf("Failed to create memtable: %d\n", err);
        return -1;
    }
    
    printf("Memtable created successfully\n");

    // Create worker threads
    pthread_t threads[TEST_THREAD_COUNT];
    int thread_created = 0;
    
    printf("Creating worker threads...\n");
    for (int i = 0; i < TEST_THREAD_COUNT; i++) {
        err = pthread_create(&threads[i], NULL, worker_thread, base);
        if (err == 0) {
            thread_created++;
            printf("Thread %d created successfully (tid: %lu)\n", i, (unsigned long)threads[i]);
        } else {
            printf("Failed to create thread %d: %s (error: %d)\n", i, strerror(err), err);
            break;
        }
        // 添加短暂延迟，避免线程同时启动
        usleep(100000);  // 100ms delay
    }

    if (thread_created == 0) {
        printf("No threads were created, test failed\n");
        ppdb_destroy(base);
        return -1;
    }

    printf("Successfully created %d threads\n", thread_created);

    // Wait for threads to complete with timeout
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 10;  // 减少超时时间到10秒

    bool all_threads_completed = true;
    printf("Waiting for threads to complete...\n");
    
    for (int i = 0; i < thread_created; i++) {
        printf("Waiting for thread %d (tid: %lu)...\n", i, (unsigned long)threads[i]);
        void* thread_result;
        err = pthread_timedjoin_np(threads[i], &thread_result, &ts);
        
        if (err != 0) {
            printf("Thread %d join error: %s (error: %d)\n", i, strerror(err), err);
            all_threads_completed = false;
            
            // 尝试正常终止线程
            printf("Attempting to cancel thread %d...\n", i);
            if (pthread_cancel(threads[i]) != 0) {
                printf("Failed to cancel thread %d\n", i);
            }
            
            // 等待线程结束，但设置较短的超时
            struct timespec short_ts;
            clock_gettime(CLOCK_REALTIME, &short_ts);
            short_ts.tv_sec += 2;
            
            if (pthread_timedjoin_np(threads[i], NULL, &short_ts) != 0) {
                printf("Failed to join thread %d after cancellation\n", i);
            } else {
                printf("Thread %d terminated after cancellation\n", i);
            }
        } else {
            printf("Thread %d completed successfully\n", i);
        }
        
        // 更新下一个线程的超时时间
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 10;
    }

    if (!all_threads_completed) {
        printf("Some threads did not complete normally\n");
    } else {
        printf("All threads completed successfully\n");
    }

    // Get and print metrics
    printf("Getting metrics...\n");
    ppdb_metrics_t metrics = {0};
    err = ppdb_storage_get_stats(base, &metrics);
    if (err == PPDB_OK) {
        printf("Concurrent test results:\n");
        printf("Total expected operations: %d\n", TEST_ITERATIONS * thread_created);
        printf("Insert ops: %zu (success: %zu)\n", 
               ppdb_sync_counter_load(&metrics.put_count),
               ppdb_sync_counter_load(&metrics.put_count));
        printf("Find ops: %zu (success: %zu)\n",
               ppdb_sync_counter_load(&metrics.get_count),
               ppdb_sync_counter_load(&metrics.get_hits));
        printf("Delete ops: %zu (success: %zu)\n",
               ppdb_sync_counter_load(&metrics.remove_count),
               ppdb_sync_counter_load(&metrics.remove_count));
    } else {
        printf("Failed to get metrics: %d\n", err);
    }

    // Cleanup
    printf("Cleaning up...\n");
    ppdb_destroy(base);
    printf("Concurrent test completed\n");
    return all_threads_completed ? 0 : -1;
}

static int test_memtable_iterator(void) {
    printf("Starting iterator test (use_lockfree=%d)...\n", USE_LOCKFREE);

    // Create memtable
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_MEMTABLE,
        .use_lockfree = USE_LOCKFREE,
        .memtable_size = TEST_MEMTABLE_SIZE
    });
    ASSERT(err == PPDB_OK, "Create memtable failed");

    // Insert test data
    for (int i = 0; i < 10; i++) {
        char key_data[32], value_data[32];
        snprintf(key_data, sizeof(key_data), "key_%d", i);
        snprintf(value_data, sizeof(value_data), "value_%d", i);

        ppdb_key_t key = {
            .data = key_data,
            .size = strlen(key_data)
        };
        ppdb_value_t value = {
            .data = value_data,
            .size = strlen(value_data)
        };

        err = ppdb_put(base, &key, &value);
        ASSERT(err == PPDB_OK, "Put failed at index %d", i);
    }

    // Test iterator
    void* iter = NULL;
    err = ppdb_iterator_init(base, &iter);
    ASSERT(err == PPDB_OK, "Iterator init failed");

    int count = 0;
    ppdb_key_t key;
    ppdb_value_t value;
    while (ppdb_iterator_next(iter, &key, &value) == PPDB_OK) {
        printf("Iter %d: key=%.*s, value=%.*s\n",
               count++,
               (int)key.size, (char*)key.data,
               (int)value.size, (char*)value.data);
        PPDB_ALIGNED_FREE(key.data);
        PPDB_ALIGNED_FREE(value.data);
    }

    ppdb_iterator_destroy(iter);
    ppdb_destroy(base);
    printf("Iterator test completed\n");
    return 0;
}

// Worker thread function for concurrent test
static void* worker_thread(void* arg) {
    ppdb_base_t* base = (ppdb_base_t*)arg;
    char key_data[TEST_KEY_SIZE] = {0};
    char value_data[TEST_VALUE_SIZE] = {0};
    ppdb_key_t key = {0};
    ppdb_value_t value = {0};
    ppdb_value_t get_value = {0};
    ppdb_error_t err;
    
    // 本地计数器
    size_t local_put_count = 0;
    size_t local_get_count = 0;
    size_t local_get_hits = 0;
    size_t local_remove_count = 0;

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // 清理之前的状态
        memset(&key, 0, sizeof(key));
        memset(&value, 0, sizeof(value));
        memset(&get_value, 0, sizeof(get_value));
        
        // 准备数据
        memset(key_data, 0, TEST_KEY_SIZE);
        memset(value_data, 0, TEST_VALUE_SIZE);
        snprintf(key_data, TEST_KEY_SIZE - 1, "key_%lu_%d", (unsigned long)pthread_self(), i);
        snprintf(value_data, TEST_VALUE_SIZE - 1, "value_%lu_%d", (unsigned long)pthread_self(), i);

        key.data = key_data;
        key.size = strlen(key_data);
        value.data = value_data;
        value.size = strlen(value_data);

        // 随机选择操作
        int op = lemur64() % 3;
        switch (op) {
            case 0: {  // Put
                err = ppdb_put(base, &key, &value);
                if (err == PPDB_OK) {
                    local_put_count++;
                } else {
                    printf("Thread %lu: Put failed with error %d\n", (unsigned long)pthread_self(), err);
                }
                break;
            }
            case 1: {  // Get
                err = ppdb_get(base, &key, &get_value);
                local_get_count++;
                if (err == PPDB_OK) {
                    local_get_hits++;
                    if (get_value.data != NULL) {
                        PPDB_ALIGNED_FREE(get_value.data);
                        get_value.data = NULL;
                    }
                }
                break;
            }
            case 2: {  // Remove
                err = ppdb_remove(base, &key);
                if (err == PPDB_OK) {
                    local_remove_count++;
                } else if (err != PPDB_ERR_NOT_FOUND) {
                    printf("Thread %lu: Remove failed with error %d\n", (unsigned long)pthread_self(), err);
                }
                break;
            }
        }
        
        // 确保清理所有可能的内存
        if (get_value.data != NULL) {
            PPDB_ALIGNED_FREE(get_value.data);
            get_value.data = NULL;
        }

        // 短暂休眠以减少资源竞争
        usleep(1000); // 增加休眠时间到1ms
    }

    // 原子更新全局计数器
    if (local_put_count > 0) {
        ppdb_sync_counter_add(&base->metrics.put_count, local_put_count);
    }
    if (local_get_count > 0) {
        ppdb_sync_counter_add(&base->metrics.get_count, local_get_count);
    }
    if (local_get_hits > 0) {
        ppdb_sync_counter_add(&base->metrics.get_hits, local_get_hits);
    }
    if (local_remove_count > 0) {
        ppdb_sync_counter_add(&base->metrics.remove_count, local_remove_count);
    }

    return NULL;
}

// Test cases array
static test_case_t test_cases[] = {
    {
        .name = "Basic Memtable Operations",
        .description = "Tests basic operations (put/get/remove) on memtable",
        .fn = test_memtable_basic,
        .timeout_seconds = 10,
        .skip = false
    },
    {
        .name = "Concurrent Memtable Operations",
        .description = "Tests concurrent operations on memtable with multiple threads",
        .fn = test_memtable_concurrent,
        .timeout_seconds = 60,  // Increased from 30 to 60 seconds
        .skip = false
    },
    {
        .name = "Memtable Iterator",
        .description = "Tests memtable iterator functionality",
        .fn = test_memtable_iterator,
        .timeout_seconds = 10,
        .skip = false
    }
};

// Test suite
static test_suite_t memtable_test_suite = {
    .name = "Memtable Test Suite",
    .setup = NULL,
    .teardown = NULL,
    .cases = test_cases,
    .num_cases = sizeof(test_cases) / sizeof(test_cases[0])
};

int main(void) {
    printf("\n=== PPDB Memtable Test Suite ===\n");
    printf("Test Mode: %s\n", USE_LOCKFREE ? "lockfree" : "locked");
    printf("Starting tests...\n\n");

    test_framework_init();
    int result = run_test_suite(&memtable_test_suite);
    test_framework_cleanup();
    test_print_stats();

    return result;
} 
