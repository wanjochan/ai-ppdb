#include <cosmopolitan.h>
#include "ppdb/sync.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "test/white/test_framework.h"
#include "test/white/infra/test_sync.h"

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
        while (true) {
            ppdb_error_t err = ppdb_sync_try_lock(data->sync);
            if (err == PPDB_OK) {
                atomic_fetch_add(data->counter, 1);
                ppdb_sync_unlock(data->sync);
                if (i % 100 == 0) {  // 每100次迭代输出一次日志
                    PPDB_LOG_INFO("Thread completed %d iterations", i);
                }
                break;
            } else if (err == PPDB_ERR_BUSY) {
                usleep(1);
            } else {
                PPDB_LOG_ERROR("Thread error: %d", err);
                return NULL;
            }
        }
    }
    PPDB_LOG_INFO("Thread completed all %d iterations", data->num_iterations);
    return NULL;
}

// 读写锁测试线程函数 - 读线程
static void* rwlock_read_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        ppdb_error_t err = ppdb_sync_read_lock(data->sync);
        if (err == PPDB_OK) {
            int value = atomic_load(data->counter);
            (void)value;
            ppdb_sync_read_unlock(data->sync);
            if (i % 100 == 0) {
                PPDB_LOG_INFO("Read thread completed %d iterations", i);
            }
        } else {
            PPDB_LOG_ERROR("Read thread error: %d", err);
            return NULL;
        }
        // 添加一个小的延迟，避免过度竞争
        if (i % 10 == 0) {
            usleep(1);
        }
    }
    PPDB_LOG_INFO("Read thread completed all %d iterations", data->num_iterations);
    return NULL;
}

// 读写锁测试线程函数 - 写线程
static void* rwlock_write_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        ppdb_error_t err = ppdb_sync_write_lock(data->sync);
        if (err == PPDB_OK) {
            atomic_fetch_add(data->counter, 1);
            ppdb_sync_write_unlock(data->sync);
            if (i % 50 == 0) {
                PPDB_LOG_INFO("Write thread completed %d iterations", i);
            }
        } else {
            PPDB_LOG_ERROR("Write thread error: %d", err);
            return NULL;
        }
        // 添加一个小的延迟，避免过度竞争
        if (i % 5 == 0) {
            usleep(1);
        }
    }
    PPDB_LOG_INFO("Write thread completed all %d iterations", data->num_iterations);
    return NULL;
}

// 测试无锁同步原语
void test_sync_lockfree(void) {
    ppdb_sync_t* sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = true,
        .enable_fairness = true,
        .enable_ref_count = false,
        .spin_count = 1000,
        .backoff_us = 1,
        .max_readers = 32
    };

    // 创建同步原语
    assert(ppdb_sync_create(&sync, &config) == PPDB_OK);

    // 测试基本锁操作
    test_sync_basic(sync);

    // 测试读写锁操作
    test_rwlock(sync);

    // 测试并发读写锁操作
    test_rwlock_concurrent(sync);

    // 销毁同步原语
    assert(ppdb_sync_destroy(sync) == PPDB_OK);
    free(sync);
}

// 测试有锁同步原语
void test_sync_locked(void) {
    ppdb_sync_t* sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = false,
        .enable_fairness = true,
        .enable_ref_count = false,
        .spin_count = 1000,
        .backoff_us = 1,
        .max_readers = 32
    };

    // 创建同步原语
    assert(ppdb_sync_create(&sync, &config) == PPDB_OK);

    // 测试基本锁操作
    test_sync_basic(sync);

    // 测试读写锁操作
    test_rwlock(sync);

    // 测试并发读写锁操作
    test_rwlock_concurrent(sync);

    // 销毁同步原语
    assert(ppdb_sync_destroy(sync) == PPDB_OK);
    free(sync);
}

// 测试基本锁操作
void test_sync_basic(ppdb_sync_t* sync) {
    // 测试锁定和解锁
    assert(ppdb_sync_try_lock(sync) == PPDB_OK);
    assert(ppdb_sync_unlock(sync) == PPDB_OK);

    // 测试重复锁定
    assert(ppdb_sync_try_lock(sync) == PPDB_OK);
    assert(ppdb_sync_try_lock(sync) == PPDB_ERR_BUSY);
    assert(ppdb_sync_unlock(sync) == PPDB_OK);
}

// 测试读写锁基本操作
void test_rwlock(ppdb_sync_t* sync) {
    // 测试读锁
    assert(ppdb_sync_read_lock(sync) == PPDB_OK);
    assert(ppdb_sync_read_lock(sync) == PPDB_OK);  // 多个读者
    assert(ppdb_sync_read_unlock(sync) == PPDB_OK);
    assert(ppdb_sync_read_unlock(sync) == PPDB_OK);

    // 测试写锁
    assert(ppdb_sync_write_lock(sync) == PPDB_OK);
    assert(ppdb_sync_read_lock(sync) == PPDB_ERR_BUSY);  // 有写者时不能读
    assert(ppdb_sync_write_unlock(sync) == PPDB_OK);

    // 测试读写互斥
    assert(ppdb_sync_read_lock(sync) == PPDB_OK);
    assert(ppdb_sync_write_lock(sync) == PPDB_ERR_BUSY);  // 有读者时不能写
    assert(ppdb_sync_read_unlock(sync) == PPDB_OK);
}

// 测试并发读写锁操作
void test_rwlock_concurrent(ppdb_sync_t* sync) {
    PPDB_LOG_INFO("Testing concurrent rwlock...");
    
    #define NUM_READERS 8
    #define NUM_WRITERS 2
    #define READ_ITERATIONS 500
    #define WRITE_ITERATIONS 100
    
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    thread_data_t reader_data[NUM_READERS];
    thread_data_t writer_data[NUM_WRITERS];
    atomic_int counter = 0;
    
    // 创建读线程
    for (int i = 0; i < NUM_READERS; i++) {
        reader_data[i].sync = sync;
        reader_data[i].counter = &counter;
        reader_data[i].num_iterations = READ_ITERATIONS;
        int ret = pthread_create(&readers[i], NULL, rwlock_read_thread, &reader_data[i]);
        assert(ret == 0);
    }
    
    // 创建写线程
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_data[i].sync = sync;
        writer_data[i].counter = &counter;
        writer_data[i].num_iterations = WRITE_ITERATIONS;
        int ret = pthread_create(&writers[i], NULL, rwlock_write_thread, &writer_data[i]);
        assert(ret == 0);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_READERS; i++) {
        int ret = pthread_join(readers[i], NULL);
        assert(ret == 0);
    }
    
    for (int i = 0; i < NUM_WRITERS; i++) {
        int ret = pthread_join(writers[i], NULL);
        assert(ret == 0);
    }
    
    assert(atomic_load(&counter) == NUM_WRITERS * WRITE_ITERATIONS);
    PPDB_LOG_INFO("Concurrent rwlock test passed");
}

// 主测试函数
int main(void) {
    // 从环境变量获取测试模式
    const char* test_mode = getenv("PPDB_SYNC_MODE");
    if (!test_mode) {
        PPDB_LOG_ERROR("PPDB_SYNC_MODE environment variable not set");
        return 1;
    }

    if (strcmp(test_mode, "lockfree") == 0) {
        PPDB_LOG_INFO("Testing lockfree version...");
        test_sync_lockfree();
    } else if (strcmp(test_mode, "locked") == 0) {
        PPDB_LOG_INFO("Testing locked version...");
        test_sync_locked();
    } else {
        PPDB_LOG_ERROR("Invalid PPDB_SYNC_MODE: %s", test_mode);
        return 1;
    }
    
    PPDB_LOG_INFO("All tests passed!");
    return 0;
}