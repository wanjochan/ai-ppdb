#include "cosmopolitan.h"
#include "kvstore/internal/sync.h"
#include "ppdb/ppdb_error.h"
#include "test/white/test_framework.h"

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
void test_mutex(void) {
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX
    };
    
    // 基本操作测试
    ppdb_error_t err = ppdb_sync_init(&sync, &config);
    ASSERT_EQ(err, PPDB_OK);
    
    // 加锁解锁测试
    err = ppdb_sync_lock(&sync);
    ASSERT_EQ(err, PPDB_OK);
    
    err = ppdb_sync_unlock(&sync);
    ASSERT_EQ(err, PPDB_OK);
    
    // try_lock测试
    bool locked = ppdb_sync_try_lock(&sync);
    ASSERT_TRUE(locked);
    if (locked) {
        err = ppdb_sync_unlock(&sync);
        ASSERT_EQ(err, PPDB_OK);
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
        pthread_create(&threads[i], NULL, mutex_thread_func, &thread_data[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    ASSERT_EQ(counter, NUM_THREADS * ITERATIONS_PER_THREAD);
    
    ppdb_sync_destroy(&sync);
}

// 自旋锁测试
void test_spinlock(void) {
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_SPINLOCK
    };
    
    // 基本操作测试
    ppdb_error_t err = ppdb_sync_init(&sync, &config);
    ASSERT_EQ(err, PPDB_OK);
    
    // 加锁解锁测试
    err = ppdb_sync_lock(&sync);
    ASSERT_EQ(err, PPDB_OK);
    
    err = ppdb_sync_unlock(&sync);
    ASSERT_EQ(err, PPDB_OK);
    
    // 多线程竞争测试
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];
    int counter = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].sync = &sync;
        thread_data[i].counter = &counter;
        thread_data[i].num_iterations = ITERATIONS_PER_THREAD;
        pthread_create(&threads[i], NULL, mutex_thread_func, &thread_data[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    ASSERT_EQ(counter, NUM_THREADS * ITERATIONS_PER_THREAD);
    
    ppdb_sync_destroy(&sync);
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
void test_rwlock(void) {
    ppdb_sync_t sync;
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK
    };
    
    // 基本操作测试
    ppdb_error_t err = ppdb_sync_init(&sync, &config);
    ASSERT_EQ(err, PPDB_OK);
    
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
        pthread_create(&readers[i], NULL, rwlock_read_thread, &reader_data[i]);
    }
    
    // 创建写线程
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_data[i].sync = &sync;
        writer_data[i].counter = &counter;
        writer_data[i].num_iterations = WRITE_ITERATIONS;
        pthread_create(&writers[i], NULL, rwlock_write_thread, &writer_data[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }
    
    ASSERT_EQ(counter, NUM_WRITERS * WRITE_ITERATIONS);
    
    ppdb_sync_destroy(&sync);
}

// 文件同步测试
void test_file_sync(void) {
    // 创建临时文件
    const char* test_file = "test_sync.tmp";
    FILE* fp = fopen(test_file, "w");
    ASSERT_TRUE(fp != NULL);
    fprintf(fp, "test data");
    fclose(fp);
    
    // 测试文件同步
    ppdb_error_t err = ppdb_sync_file(test_file);
    ASSERT_EQ(err, PPDB_OK);
    
    // 测试文件描述符同步
    fp = fopen(test_file, "r");
    ASSERT_TRUE(fp != NULL);
    err = ppdb_sync_fd(fileno(fp));
    ASSERT_EQ(err, PPDB_OK);
    fclose(fp);
    
    // 清理
    remove(test_file);
}

int main(void) {
    TEST_INIT("Sync Primitives Test");
    
    RUN_TEST(test_mutex);
    RUN_TEST(test_spinlock);
    RUN_TEST(test_rwlock);
    RUN_TEST(test_file_sync);
    
    TEST_SUMMARY();
    return TEST_RESULT();
}