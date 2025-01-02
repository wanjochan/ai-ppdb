#include <cosmopolitan.h>
#include "ppdb/sync.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "test/white/test_framework.h"

// 测试线程数据结构
typedef struct {
    ppdb_sync_t* sync;
    atomic_int* counter;
    int num_iterations;
} thread_data_t;

// 互斥锁竞争测试的线程函数
static void* mutex_thread_func(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        ppdb_error_t err = ppdb_sync_try_lock(data->sync);
        if (err == PPDB_OK) {
            atomic_fetch_add(data->counter, 1);
            ppdb_sync_unlock(data->sync);
        }
    }
    return NULL;
}

// 基础同步原语测试
static int test_sync_basic(bool use_lockfree) {
    PPDB_LOG_INFO("Testing sync basic (lockfree=%d)...", use_lockfree);
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .use_lockfree = use_lockfree,
        .stripe_count = 1,
        .backoff_us = use_lockfree ? 1 : 100,
        .enable_ref_count = false,
        .retry_count = 100,
        .retry_delay_us = 1
    };
    
    // 基本操作测试
    ppdb_error_t err = ppdb_sync_init(&sync, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to initialize mutex");
    
    // 加锁解锁测试
    err = ppdb_sync_try_lock(&sync);
    TEST_ASSERT(err == PPDB_OK, "Failed to lock mutex");
    
    err = ppdb_sync_unlock(&sync);
    TEST_ASSERT(err == PPDB_OK, "Failed to unlock mutex");
    
    // try_lock测试
    err = ppdb_sync_try_lock(&sync);
    TEST_ASSERT(err == PPDB_OK, "Failed to try_lock mutex");
    
    if (err == PPDB_OK) {
        err = ppdb_sync_unlock(&sync);
        TEST_ASSERT(err == PPDB_OK, "Failed to unlock mutex after try_lock");
    }

    // 多线程竞争测试
    #define NUM_THREADS 4
    #define ITERATIONS_PER_THREAD 10000
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    atomic_int counter = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].sync = &sync;
        thread_data[i].counter = &counter;
        thread_data[i].num_iterations = ITERATIONS_PER_THREAD;
        int ret = pthread_create(&threads[i], NULL, mutex_thread_func, &thread_data[i]);
        TEST_ASSERT(ret == 0, "Failed to create thread");
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_join(threads[i], NULL);
        TEST_ASSERT(ret == 0, "Failed to join thread");
    }
    
    TEST_ASSERT(atomic_load(&counter) == NUM_THREADS * ITERATIONS_PER_THREAD, "Counter value mismatch");
    
    err = ppdb_sync_destroy(&sync);
    TEST_ASSERT(err == PPDB_OK, "Failed to destroy mutex");
    return 0;
}

// 读写锁测试线程函数 - 读线程
static void* rwlock_read_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        ppdb_sync_read_lock(data->sync);
        int value = atomic_load(data->counter);  // 只读取，不修改
        (void)value;  // 避免未使用警告
        ppdb_sync_read_unlock(data->sync);
    }
    return NULL;
}

// 读写锁测试线程函数 - 写线程
static void* rwlock_write_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        ppdb_error_t err = ppdb_sync_try_lock(data->sync);
        if (err == PPDB_OK) {
            atomic_fetch_add(data->counter, 1);
            ppdb_sync_unlock(data->sync);
        }
    }
    return NULL;
}

// 读写锁测试
static int test_rwlock(bool use_lockfree) {
    PPDB_LOG_INFO("Testing rwlock (lockfree=%d)...", use_lockfree);
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .spin_count = 1000,
        .use_lockfree = use_lockfree,
        .stripe_count = 1,
        .backoff_us = use_lockfree ? 1 : 100,
        .enable_ref_count = false
    };
    
    // 基本操作测试
    ppdb_error_t err = ppdb_sync_init(&sync, &config);
    TEST_ASSERT(err == PPDB_OK, "Failed to initialize rwlock");
    
    // 读写锁测试
    #define NUM_READERS 8
    #define NUM_WRITERS 2
    #define READ_ITERATIONS 5000
    #define WRITE_ITERATIONS 1000
    
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    thread_data_t reader_data[NUM_READERS];
    thread_data_t writer_data[NUM_WRITERS];
    atomic_int counter = 0;
    
    // 创建读线程
    for (int i = 0; i < NUM_READERS; i++) {
        reader_data[i].sync = &sync;
        reader_data[i].counter = &counter;
        reader_data[i].num_iterations = READ_ITERATIONS;
        int ret = pthread_create(&readers[i], NULL, rwlock_read_thread, &reader_data[i]);
        TEST_ASSERT(ret == 0, "Failed to create reader thread");
    }
    
    // 创建写线程
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_data[i].sync = &sync;
        writer_data[i].counter = &counter;
        writer_data[i].num_iterations = WRITE_ITERATIONS;
        int ret = pthread_create(&writers[i], NULL, rwlock_write_thread, &writer_data[i]);
        TEST_ASSERT(ret == 0, "Failed to create writer thread");
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_READERS; i++) {
        int ret = pthread_join(readers[i], NULL);
        TEST_ASSERT(ret == 0, "Failed to join reader thread");
    }
    
    for (int i = 0; i < NUM_WRITERS; i++) {
        int ret = pthread_join(writers[i], NULL);
        TEST_ASSERT(ret == 0, "Failed to join writer thread");
    }
    
    TEST_ASSERT(atomic_load(&counter) == NUM_WRITERS * WRITE_ITERATIONS, "Counter value mismatch");
    
    err = ppdb_sync_destroy(&sync);
    TEST_ASSERT(err == PPDB_OK, "Failed to destroy rwlock");
    return 0;
}

// 主测试函数
int main(void) {
    // 从环境变量获取测试模式
    const char* test_mode = getenv("PPDB_SYNC_MODE");
    bool use_lockfree = (test_mode && strcmp(test_mode, "lockfree") == 0);
    
    PPDB_LOG_INFO("Testing %s version...", use_lockfree ? "lockfree" : "locked");
    
    // 执行测试
    test_sync_basic(use_lockfree);
    test_rwlock(use_lockfree);
    
    return 0;
}