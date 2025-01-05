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

int main() {
    RUN_TEST(test_sync_basic);
    RUN_TEST(test_rwlock);
    RUN_TEST(test_sync);
    return 0;
}
