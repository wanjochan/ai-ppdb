#include <cosmopolitan.h>
#include "../test_framework.h"
#include "internal/base.h"

// 测试互斥锁基本功能
static int test_mutex_basic(void) {
    ppdb_base_mutex_t* mutex = NULL;
    TEST_ASSERT_EQUALS(ppdb_base_mutex_create(&mutex), PPDB_OK);
    TEST_ASSERT_NOT_NULL(mutex);
    
    TEST_ASSERT_EQUALS(ppdb_base_mutex_lock(mutex), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_mutex_unlock(mutex), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_mutex_destroy(mutex), PPDB_OK);
    return 0;
}

// 测试互斥锁trylock
static int test_mutex_trylock(void) {
    ppdb_base_mutex_t* mutex = NULL;
    TEST_ASSERT_EQUALS(ppdb_base_mutex_create(&mutex), PPDB_OK);
    TEST_ASSERT_NOT_NULL(mutex);
    
    TEST_ASSERT_EQUALS(ppdb_base_mutex_trylock(mutex), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_mutex_unlock(mutex), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_mutex_destroy(mutex), PPDB_OK);
    return 0;
}

// 测试读写锁基本功能
static int test_rwlock_basic(void) {
    ppdb_base_rwlock_t* rwlock = NULL;
    TEST_ASSERT_EQUALS(ppdb_base_rwlock_create(&rwlock), PPDB_OK);
    TEST_ASSERT_NOT_NULL(rwlock);
    
    TEST_ASSERT_EQUALS(ppdb_base_rwlock_rdlock(rwlock), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_rwlock_unlock(rwlock), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_rwlock_wrlock(rwlock), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_rwlock_unlock(rwlock), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_rwlock_destroy(rwlock), PPDB_OK);
    return 0;
}

// 测试条件变量基本功能
static int test_cond_basic(void) {
    ppdb_base_mutex_t* mutex = NULL;
    ppdb_base_cond_t* cond = NULL;
    
    TEST_ASSERT_EQUALS(ppdb_base_mutex_create(&mutex), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_cond_create(&cond), PPDB_OK);
    
    TEST_ASSERT_EQUALS(ppdb_base_mutex_lock(mutex), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_cond_signal(cond), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_mutex_unlock(mutex), PPDB_OK);
    
    TEST_ASSERT_EQUALS(ppdb_base_cond_destroy(cond), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_mutex_destroy(mutex), PPDB_OK);
    return 0;
}

// 测试信号量基本功能
static int test_sem_basic(void) {
    ppdb_base_sem_t* sem = NULL;
    TEST_ASSERT_EQUALS(ppdb_base_sem_create(&sem, 1), PPDB_OK);
    TEST_ASSERT_NOT_NULL(sem);
    
    TEST_ASSERT_EQUALS(ppdb_base_sem_wait(sem), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_sem_post(sem), PPDB_OK);
    TEST_ASSERT_EQUALS(ppdb_base_sem_destroy(sem), PPDB_OK);
    return 0;
}

// 测试原子操作
static int test_atomic_basic(void) {
    ppdb_base_atomic_t value = PPDB_BASE_ATOMIC_INIT(0);
    
    TEST_ASSERT_EQUALS(0, ppdb_base_atomic_load(&value));
    ppdb_base_atomic_store(&value, 42);
    TEST_ASSERT_EQUALS(42, ppdb_base_atomic_load(&value));
    TEST_ASSERT_EQUALS(42, ppdb_base_atomic_fetch_add(&value, 8));
    TEST_ASSERT_EQUALS(50, ppdb_base_atomic_load(&value));
    TEST_ASSERT_EQUALS(50, ppdb_base_atomic_fetch_sub(&value, 10));
    TEST_ASSERT_EQUALS(40, ppdb_base_atomic_load(&value));
    
    uint64_t expected = 40;
    TEST_ASSERT_TRUE(ppdb_base_atomic_compare_exchange(&value, &expected, 100));
    TEST_ASSERT_EQUALS(100, ppdb_base_atomic_load(&value));
    return 0;
}

// 测试多线程互斥锁
static void* mutex_thread_func(void* arg) {
    ppdb_base_mutex_t* mutex = arg;
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_mutex_lock(mutex), PPDB_OK);
        TEST_ASSERT_EQUALS(ppdb_base_mutex_unlock(mutex), PPDB_OK);
    }
    return NULL;
}

static int test_mutex_threads(void) {
    ppdb_base_mutex_t* mutex = NULL;
    ppdb_base_thread_t threads[10];
    
    TEST_ASSERT_EQUALS(ppdb_base_mutex_create(&mutex), PPDB_OK);
    
    // 创建10个线程
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_thread_create(&threads[i], mutex_thread_func, mutex), PPDB_OK);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_thread_join(threads[i], NULL), PPDB_OK);
    }
    
    TEST_ASSERT_EQUALS(ppdb_base_mutex_destroy(mutex), PPDB_OK);
    return 0;
}

// 测试多线程读写锁
static void* rwlock_reader_func(void* arg) {
    ppdb_base_rwlock_t* rwlock = arg;
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_rwlock_rdlock(rwlock), PPDB_OK);
        // 模拟读操作
        ppdb_base_sleep_ms(1);
        TEST_ASSERT_EQUALS(ppdb_base_rwlock_unlock(rwlock), PPDB_OK);
    }
    return NULL;
}

static void* rwlock_writer_func(void* arg) {
    ppdb_base_rwlock_t* rwlock = arg;
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_rwlock_wrlock(rwlock), PPDB_OK);
        // 模拟写操作
        ppdb_base_sleep_ms(10);
        TEST_ASSERT_EQUALS(ppdb_base_rwlock_unlock(rwlock), PPDB_OK);
    }
    return NULL;
}

static int test_rwlock_threads(void) {
    ppdb_base_rwlock_t* rwlock = NULL;
    ppdb_base_thread_t readers[8];
    ppdb_base_thread_t writers[2];
    
    // 初始化读写锁
    TEST_ASSERT_EQUALS(ppdb_base_rwlock_create(&rwlock), PPDB_OK);
    
    // 创建8个读线程
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_thread_create(&readers[i], rwlock_reader_func, rwlock), PPDB_OK);
    }
    
    // 创建2个写线程
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_thread_create(&writers[i], rwlock_writer_func, rwlock), PPDB_OK);
    }
    
    // 等待读线程完成
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_thread_join(readers[i], NULL), PPDB_OK);
    }
    
    // 等待写线程完成
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUALS(ppdb_base_thread_join(writers[i], NULL), PPDB_OK);
    }
    
    // 销毁读写锁
    TEST_ASSERT_EQUALS(ppdb_base_rwlock_destroy(rwlock), PPDB_OK);
    return 0;
}

int main(void) {
    TEST_INIT();
    
    TEST_RUN(test_mutex_basic);
    TEST_RUN(test_mutex_trylock);
    TEST_RUN(test_rwlock_basic);
    TEST_RUN(test_cond_basic);
    TEST_RUN(test_sem_basic);
    TEST_RUN(test_atomic_basic);
    TEST_RUN(test_mutex_threads);
    TEST_RUN(test_rwlock_threads);
    
    TEST_REPORT();
    return 0;
}