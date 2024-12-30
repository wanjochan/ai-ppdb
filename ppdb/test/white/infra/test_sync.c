#include <cosmopolitan.h>
#include "kvstore/internal/sync.h"
#include "ppdb/ppdb_error.h"
#include "test_framework.h"

// 测试线程数据结构
typedef struct {
    ppdb_sync_t* sync;
    int* counter;
    int num_iterations;
} thread_data_t;

// 互斥锁竞争测试的线程函数
static void* mutex_thread_func(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        ppdb_sync_lock(data->sync);
        (*data->counter)++;
        ppdb_sync_unlock(data->sync);
    }
    return NULL;
}

// 互斥锁测试
int test_mutex(void) {
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    
    // 基本操作测试
    ppdb_error_t err = ppdb_sync_init(&sync, &config);
    ppdb_log_info("Mutex init result: %s (%d)", ppdb_error_string(err), err);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to initialize mutex: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    // 加锁解锁测试
    err = ppdb_sync_lock(&sync);
    ppdb_log_info("Mutex lock result: %s (%d)", ppdb_error_string(err), err);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to lock mutex: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    err = ppdb_sync_unlock(&sync);
    ppdb_log_info("Mutex unlock result: %s (%d)", ppdb_error_string(err), err);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to unlock mutex: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    // try_lock测试
    bool locked = ppdb_sync_try_lock(&sync);
    ppdb_log_info("Mutex try_lock result: %d", locked);
    if (!locked) {
        ppdb_log_error("Failed to try_lock mutex");
        ASSERT_TRUE(locked);
        return -1;
    }
    
    if (locked) {
        err = ppdb_sync_unlock(&sync);
        ppdb_log_info("Mutex unlock result: %s (%d)", ppdb_error_string(err), err);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to unlock mutex after try_lock: %s (%d)", ppdb_error_string(err), err);
            ASSERT_EQ(err, PPDB_OK);
            return -1;
        }
    }

    // 多线程竞争测试
    #define NUM_THREADS 4
    #define ITERATIONS_PER_THREAD 10000
    
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    int counter = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].sync = &sync;
        thread_data[i].counter = &counter;
        thread_data[i].num_iterations = ITERATIONS_PER_THREAD;
        int ret = pthread_create(&threads[i], NULL, mutex_thread_func, &thread_data[i]);
        if (ret != 0) {
            ppdb_log_error("Failed to create thread %d: %d", i, ret);
            ASSERT_EQ(ret, 0);
            return -1;
        }
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_join(threads[i], NULL);
        if (ret != 0) {
            ppdb_log_error("Failed to join thread %d: %d", i, ret);
            ASSERT_EQ(ret, 0);
            return -1;
        }
    }
    
    ASSERT_EQ(counter, NUM_THREADS * ITERATIONS_PER_THREAD);
    
    err = ppdb_sync_destroy(&sync);
    ppdb_log_info("Mutex destroy result: %s (%d)", ppdb_error_string(err), err);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to destroy mutex: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    return 0;
}

// 自旋锁测试
int test_spinlock(void) {
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_SPINLOCK,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    
    // 基本操作测试
    ppdb_error_t err = ppdb_sync_init(&sync, &config);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to initialize spinlock: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    // 加锁解锁测试
    err = ppdb_sync_lock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to lock spinlock: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    err = ppdb_sync_unlock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to unlock spinlock: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    // 多线程竞争测试
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    int counter = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].sync = &sync;
        thread_data[i].counter = &counter;
        thread_data[i].num_iterations = ITERATIONS_PER_THREAD;
        int ret = pthread_create(&threads[i], NULL, mutex_thread_func, &thread_data[i]);
        if (ret != 0) {
            ppdb_log_error("Failed to create thread %d: %d", i, ret);
            ASSERT_EQ(ret, 0);
            return -1;
        }
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_join(threads[i], NULL);
        if (ret != 0) {
            ppdb_log_error("Failed to join thread %d: %d", i, ret);
            ASSERT_EQ(ret, 0);
            return -1;
        }
    }
    
    ASSERT_EQ(counter, NUM_THREADS * ITERATIONS_PER_THREAD);
    
    err = ppdb_sync_destroy(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to destroy spinlock: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    return 0;
}

// 读写锁测试线程函数 - 读线程
static void* rwlock_read_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        ppdb_sync_read_lock(data->sync);
        int value = *data->counter;  // 只读取，不修改
        (void)value;  // 避免未使用警告
        ppdb_sync_read_unlock(data->sync);
    }
    return NULL;
}

// 读写锁测试线程函数 - 写线程
static void* rwlock_write_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    for (int i = 0; i < data->num_iterations; i++) {
        ppdb_sync_lock(data->sync);
        (*data->counter)++;
        ppdb_sync_unlock(data->sync);
    }
    return NULL;
}

// 读写锁测试
int test_rwlock(void) {
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    
    // 基本操作测试
    ppdb_error_t err = ppdb_sync_init(&sync, &config);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to initialize rwlock: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    // 读写锁测试
    #define NUM_READERS 8
    #define NUM_WRITERS 2
    #define READ_ITERATIONS 5000
    #define WRITE_ITERATIONS 1000
    
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    thread_data_t reader_data[NUM_READERS];
    thread_data_t writer_data[NUM_WRITERS];
    int counter = 0;
    
    // 创建读线程
    for (int i = 0; i < NUM_READERS; i++) {
        reader_data[i].sync = &sync;
        reader_data[i].counter = &counter;
        reader_data[i].num_iterations = READ_ITERATIONS;
        int ret = pthread_create(&readers[i], NULL, rwlock_read_thread, &reader_data[i]);
        if (ret != 0) {
            ppdb_log_error("Failed to create reader thread %d: %d", i, ret);
            ASSERT_EQ(ret, 0);
            return -1;
        }
    }
    
    // 创建写线程
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_data[i].sync = &sync;
        writer_data[i].counter = &counter;
        writer_data[i].num_iterations = WRITE_ITERATIONS;
        int ret = pthread_create(&writers[i], NULL, rwlock_write_thread, &writer_data[i]);
        if (ret != 0) {
            ppdb_log_error("Failed to create writer thread %d: %d", i, ret);
            ASSERT_EQ(ret, 0);
            return -1;
        }
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_READERS; i++) {
        int ret = pthread_join(readers[i], NULL);
        if (ret != 0) {
            ppdb_log_error("Failed to join reader thread %d: %d", i, ret);
            ASSERT_EQ(ret, 0);
            return -1;
        }
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        int ret = pthread_join(writers[i], NULL);
        if (ret != 0) {
            ppdb_log_error("Failed to join writer thread %d: %d", i, ret);
            ASSERT_EQ(ret, 0);
            return -1;
        }
    }
    
    ASSERT_EQ(counter, NUM_WRITERS * WRITE_ITERATIONS);
    
    err = ppdb_sync_destroy(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to destroy rwlock: %s (%d)", ppdb_error_string(err), err);
        ASSERT_EQ(err, PPDB_OK);
        return -1;
    }
    
    return 0;
}

// 文件同步测试
int test_file_sync(void) {
    // 创建临时文件
    const char* test_file = "test_sync.tmp";
    int fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        ppdb_log_error("Failed to create test file: %s (errno: %d)", test_file, errno);
        ASSERT_TRUE(fd >= 0);
        return -1;
    }
    ppdb_log_info("Created test file: %s", test_file);
    
    // 写入一些数据
    const char* data = "test data\n";
    ssize_t written = write(fd, data, strlen(data));
    if (written != (ssize_t)strlen(data)) {
        ppdb_log_error("Failed to write test data (errno: %d)", errno);
        ASSERT_EQ(written, (ssize_t)strlen(data));
        close(fd);
        remove(test_file);
        return -1;
    }
    close(fd);
    ppdb_log_info("Wrote test data to file");
    
    // 测试文件同步
    ppdb_error_t err = ppdb_sync_file(test_file);
    ppdb_log_info("File sync result: %s (%d)", ppdb_error_string(err), err);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to sync file: %s (%s)", test_file, ppdb_error_string(err));
        ASSERT_EQ(err, PPDB_OK);
        remove(test_file);
        return -1;
    }
    
    // 测试文件描述符同步
    fd = open(test_file, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        ppdb_log_error("Failed to open test file for reading: %s (errno: %d)", test_file, errno);
        ASSERT_TRUE(fd >= 0);
        remove(test_file);
        return -1;
    }
    ppdb_log_info("Opened test file for reading");
    
    err = ppdb_sync_fd(fd);
    ppdb_log_info("File descriptor sync result: %s (%d)", ppdb_error_string(err), err);
    close(fd);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to sync file descriptor: %d (%s)", fd, ppdb_error_string(err));
        ASSERT_EQ(err, PPDB_OK);
        remove(test_file);
        return -1;
    }
    
    // 清理
    ppdb_log_info("Cleaning up test file");
    remove(test_file);
    return 0;
}

// 测试套件定义
static const test_case_t sync_test_cases[] = {
    {"mutex", test_mutex, 30, false, "Test mutex basic functionality"},
    {"spinlock", test_spinlock, 30, false, "Test spinlock basic functionality"},
    {"rwlock", test_rwlock, 30, false, "Test rwlock basic functionality"},
    {"file_sync", test_file_sync, 30, false, "Test file sync functionality"}
};

static const test_suite_t sync_test_suite = {
    .name = "Sync Primitives Test",
    .cases = sync_test_cases,
    .num_cases = sizeof(sync_test_cases) / sizeof(sync_test_cases[0]),
    .setup = NULL,
    .teardown = NULL
};

// 主函数
int main(void) {
    test_framework_init();
    int result = run_test_suite(&sync_test_suite);
    test_framework_cleanup();
    return result;
}