#include "test_common.h"
#include "internal/engine.h"

static void test_mutex_basic(void) {
    ppdb_engine_mutex_t* mutex;
    TEST_ASSERT_OK(ppdb_engine_mutex_create(&mutex));
    TEST_ASSERT_OK(ppdb_engine_mutex_lock(mutex));
    TEST_ASSERT_OK(ppdb_engine_mutex_unlock(mutex));
    TEST_ASSERT_OK(ppdb_engine_mutex_destroy(mutex));
}

static void test_mutex_trylock(void) {
    ppdb_engine_mutex_t* mutex;
    TEST_ASSERT_OK(ppdb_engine_mutex_create(&mutex));
    TEST_ASSERT_OK(ppdb_engine_mutex_trylock(mutex));
    TEST_ASSERT_OK(ppdb_engine_mutex_unlock(mutex));
    TEST_ASSERT_OK(ppdb_engine_mutex_destroy(mutex));
}

static void test_rwlock_basic(void) {
    ppdb_engine_rwlock_t* rwlock;
    TEST_ASSERT_OK(ppdb_engine_rwlock_create(&rwlock));
    TEST_ASSERT_OK(ppdb_engine_rwlock_rdlock(rwlock));
    TEST_ASSERT_OK(ppdb_engine_rwlock_unlock(rwlock));
    TEST_ASSERT_OK(ppdb_engine_rwlock_wrlock(rwlock));
    TEST_ASSERT_OK(ppdb_engine_rwlock_unlock(rwlock));
    TEST_ASSERT_OK(ppdb_engine_rwlock_destroy(rwlock));
}

static void test_cond_basic(void) {
    ppdb_engine_mutex_t* mutex;
    ppdb_engine_cond_t* cond;
    TEST_ASSERT_OK(ppdb_engine_mutex_create(&mutex));
    TEST_ASSERT_OK(ppdb_engine_cond_create(&cond));
    TEST_ASSERT_OK(ppdb_engine_mutex_lock(mutex));
    TEST_ASSERT_OK(ppdb_engine_cond_signal(cond));
    TEST_ASSERT_OK(ppdb_engine_mutex_unlock(mutex));
    TEST_ASSERT_OK(ppdb_engine_cond_destroy(cond));
    TEST_ASSERT_OK(ppdb_engine_mutex_destroy(mutex));
}

static void test_sem_basic(void) {
    ppdb_engine_sem_t* sem;
    TEST_ASSERT_OK(ppdb_engine_sem_create(&sem, 1));
    TEST_ASSERT_OK(ppdb_engine_sem_wait(sem));
    TEST_ASSERT_OK(ppdb_engine_sem_post(sem));
    TEST_ASSERT_OK(ppdb_engine_sem_destroy(sem));
}

static void test_atomic_ops(void) {
    size_t value = 0;
    TEST_ASSERT_EQUAL(0, ppdb_engine_atomic_load(&value));
    ppdb_engine_atomic_store(&value, 42);
    TEST_ASSERT_EQUAL(42, ppdb_engine_atomic_load(&value));
    TEST_ASSERT_EQUAL(42, ppdb_engine_atomic_add(&value, 8));
    TEST_ASSERT_EQUAL(50, ppdb_engine_atomic_load(&value));
    TEST_ASSERT_EQUAL(50, ppdb_engine_atomic_sub(&value, 10));
    TEST_ASSERT_EQUAL(40, ppdb_engine_atomic_load(&value));
    TEST_ASSERT_TRUE(ppdb_engine_atomic_cas(&value, 40, 100));
    TEST_ASSERT_EQUAL(100, ppdb_engine_atomic_load(&value));
}

static void* mutex_thread_func(void* arg) {
    ppdb_engine_mutex_t* mutex = arg;
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT_OK(ppdb_engine_mutex_lock(mutex));
        TEST_ASSERT_OK(ppdb_engine_mutex_unlock(mutex));
    }
    return NULL;
}

static void test_mutex_threaded(void) {
    ppdb_engine_mutex_t* mutex;
    ppdb_engine_thread_t* threads[10];
    
    TEST_ASSERT_OK(ppdb_engine_mutex_create(&mutex));
    
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_OK(ppdb_engine_thread_create(&threads[i], mutex_thread_func, mutex));
    }
    
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_OK(ppdb_engine_thread_join(threads[i], NULL));
    }
    
    TEST_ASSERT_OK(ppdb_engine_mutex_destroy(mutex));
}

static atomic_size_t shared_counter = 0;

static void* rwlock_reader_func(void* arg) {
    ppdb_engine_rwlock_t* rwlock = arg;
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT_OK(ppdb_engine_rwlock_rdlock(rwlock));
        atomic_load(&shared_counter);
        TEST_ASSERT_OK(ppdb_engine_rwlock_unlock(rwlock));
    }
    return NULL;
}

static void* rwlock_writer_func(void* arg) {
    ppdb_engine_rwlock_t* rwlock = arg;
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_OK(ppdb_engine_rwlock_wrlock(rwlock));
        atomic_fetch_add(&shared_counter, 1);
        TEST_ASSERT_OK(ppdb_engine_rwlock_unlock(rwlock));
    }
    return NULL;
}

static void test_rwlock_threaded(void) {
    ppdb_engine_rwlock_t* rwlock;
    ppdb_engine_thread_t* readers[8];
    ppdb_engine_thread_t* writers[2];
    
    atomic_store(&shared_counter, 0);
    TEST_ASSERT_OK(ppdb_engine_rwlock_create(&rwlock));
    
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_OK(ppdb_engine_thread_create(&readers[i], rwlock_reader_func, rwlock));
    }
    
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_OK(ppdb_engine_thread_create(&writers[i], rwlock_writer_func, rwlock));
    }
    
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_OK(ppdb_engine_thread_join(readers[i], NULL));
    }
    
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_OK(ppdb_engine_thread_join(writers[i], NULL));
    }
    
    TEST_ASSERT_EQUAL(200, atomic_load(&shared_counter));
    TEST_ASSERT_OK(ppdb_engine_rwlock_destroy(rwlock));
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_mutex_basic);
    RUN_TEST(test_mutex_trylock);
    RUN_TEST(test_rwlock_basic);
    RUN_TEST(test_cond_basic);
    RUN_TEST(test_sem_basic);
    RUN_TEST(test_atomic_ops);
    RUN_TEST(test_mutex_threaded);
    RUN_TEST(test_rwlock_threaded);
    
    return UNITY_END();
}
