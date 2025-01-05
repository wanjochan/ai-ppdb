#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "test_framework.h"
#include "test_macros.h"

static void* mutex_thread_func(void* arg) {
    ppdb_sync_t* sync = (ppdb_sync_t*)arg;
    ppdb_error_t err;
    int i;

    for (i = 0; i < 1000; i++) {
        err = ppdb_sync_lock(sync);
        ASSERT_OK(err);
        // Critical section
        usleep(1);
        err = ppdb_sync_unlock(sync);
        ASSERT_OK(err);
    }

    return NULL;
}

static void* reader_thread_func(void* arg) {
    ppdb_sync_t* sync = (ppdb_sync_t*)arg;
    ppdb_error_t err;
    int i;

    for (i = 0; i < 1000; i++) {
        err = ppdb_sync_read_lock(sync);
        ASSERT_OK(err);
        // Read section
        usleep(1);
        err = ppdb_sync_read_unlock(sync);
        ASSERT_OK(err);
    }

    return NULL;
}

static void* writer_thread_func(void* arg) {
    ppdb_sync_t* sync = (ppdb_sync_t*)arg;
    ppdb_error_t err;
    int i;

    for (i = 0; i < 100; i++) {
        err = ppdb_sync_write_lock(sync);
        ASSERT_OK(err);
        // Write section
        usleep(10);
        err = ppdb_sync_write_unlock(sync);
        ASSERT_OK(err);
    }

    return NULL;
}

TEST(test_sync) {
    ppdb_sync_t* sync;
    ppdb_error_t err;
    pthread_t threads[10];
    int i;

    // Test mutex
    ppdb_sync_config_t mutex_config = {
        .type = PPDB_SYNC_MUTEX,
        .use_lockfree = false,
        .max_readers = 1,
        .backoff_us = 1000,
        .max_retries = 100
    };

    err = ppdb_sync_create(&sync, &mutex_config);
    ASSERT_OK(err);

    for (i = 0; i < 10; i++) {
        err = pthread_create(&threads[i], NULL, mutex_thread_func, sync);
        ASSERT_OK(err);
    }

    for (i = 0; i < 10; i++) {
        err = pthread_join(threads[i], NULL);
        ASSERT_OK(err);
    }

    ppdb_sync_destroy(sync);

    // Test rwlock
    ppdb_sync_config_t rwlock_config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = false,
        .max_readers = 32,
        .backoff_us = 1000,
        .max_retries = 100
    };

    err = ppdb_sync_create(&sync, &rwlock_config);
    ASSERT_OK(err);

    // Create reader threads
    for (i = 0; i < 8; i++) {
        err = pthread_create(&threads[i], NULL, reader_thread_func, sync);
        ASSERT_OK(err);
    }

    // Create writer threads
    for (i = 8; i < 10; i++) {
        err = pthread_create(&threads[i], NULL, writer_thread_func, sync);
        ASSERT_OK(err);
    }

    for (i = 0; i < 10; i++) {
        err = pthread_join(threads[i], NULL);
        ASSERT_OK(err);
    }

    ppdb_sync_destroy(sync);
}

TEST(test_sync_basic) {
    ppdb_sync_t* sync;
    ppdb_error_t err;

    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .use_lockfree = false,
        .max_readers = 1,
        .backoff_us = 1000,
        .max_retries = 100
    };

    err = ppdb_sync_create(&sync, &config);
    ASSERT_OK(err);

    // Test basic lock/unlock
    err = ppdb_sync_lock(sync);
    ASSERT_OK(err);
    err = ppdb_sync_unlock(sync);
    ASSERT_OK(err);

    // Test try_lock
    err = ppdb_sync_try_lock(sync);
    ASSERT_OK(err);
    err = ppdb_sync_unlock(sync);
    ASSERT_OK(err);

    ppdb_sync_destroy(sync);
}

TEST(test_rwlock) {
    ppdb_sync_t* sync;
    ppdb_error_t err;

    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = false,
        .max_readers = 32,
        .backoff_us = 1000,
        .max_retries = 100
    };

    err = ppdb_sync_create(&sync, &config);
    ASSERT_OK(err);

    // Test read lock
    err = ppdb_sync_read_lock(sync);
    ASSERT_OK(err);
    err = ppdb_sync_read_unlock(sync);
    ASSERT_OK(err);

    // Test write lock
    err = ppdb_sync_write_lock(sync);
    ASSERT_OK(err);
    err = ppdb_sync_write_unlock(sync);
    ASSERT_OK(err);

    // Test try read lock
    err = ppdb_sync_try_read_lock(sync);
    ASSERT_OK(err);
    err = ppdb_sync_read_unlock(sync);
    ASSERT_OK(err);

    // Test try write lock
    err = ppdb_sync_try_write_lock(sync);
    ASSERT_OK(err);
    err = ppdb_sync_write_unlock(sync);
    ASSERT_OK(err);

    ppdb_sync_destroy(sync);
}

TEST(test_sync_stress) {
    ppdb_sync_t* sync;
    ppdb_error_t err;
    pthread_t threads[32];
    int i;

    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .use_lockfree = true,
        .max_readers = 32,
        .backoff_us = 1,
        .max_backoff_us = 100,
        .spin_count = 1000,
        .max_retries = 10000,
        .enable_stats = true
    };

    err = ppdb_sync_create(&sync, &config);
    ASSERT_OK(err);

    // 创建更多的并发线程进行压力测试
    for (i = 0; i < 24; i++) {
        err = pthread_create(&threads[i], NULL, reader_thread_func, sync);
        ASSERT_OK(err);
    }

    for (i = 24; i < 32; i++) {
        err = pthread_create(&threads[i], NULL, writer_thread_func, sync);
        ASSERT_OK(err);
    }

    for (i = 0; i < 32; i++) {
        err = pthread_join(threads[i], NULL);
        ASSERT_OK(err);
    }

    // 验证统计信息
    ASSERT_TRUE(atomic_load(&sync->stats->contention_count) > 0);
    ASSERT_TRUE(atomic_load(&sync->stats->total_wait_time_us) > 0);
    ASSERT_TRUE(atomic_load(&sync->stats->max_wait_time_us) > 0);
    ASSERT_TRUE(atomic_load(&sync->stats->concurrent_readers) > 0);

    ppdb_sync_destroy(sync);
}

TEST(test_sync_timeout) {
    ppdb_sync_t* sync;
    ppdb_error_t err;
    pthread_t thread;

    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .use_lockfree = true,
        .backoff_us = 1,
        .max_backoff_us = 10,
        .spin_count = 100,
        .max_retries = 1000
    };

    err = ppdb_sync_create(&sync, &config);
    ASSERT_OK(err);

    // 先获取锁
    err = ppdb_sync_lock(sync);
    ASSERT_OK(err);

    // 创建另一个线程尝试获取锁
    err = pthread_create(&thread, NULL, mutex_thread_func, sync);
    ASSERT_OK(err);

    // 等待足够长的时间让另一个线程超时
    usleep(100000);  // 100ms

    // 释放锁
    err = ppdb_sync_unlock(sync);
    ASSERT_OK(err);

    err = pthread_join(thread, NULL);
    ASSERT_OK(err);

    ppdb_sync_destroy(sync);
}

int main() {
    RUN_TEST(test_sync_basic);
    RUN_TEST(test_rwlock);
    RUN_TEST(test_sync);
    RUN_TEST(test_sync_stress);
    RUN_TEST(test_sync_timeout);
    return 0;
}
