#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_sync.h"

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        printf("ASSERT FAILED: %s\n", msg); \
        return 1; \
    }

static void thread_func(void* arg) {
    int* counter = (int*)arg;
    (*counter)++;
}

static int test_thread(void) {
    ppdb_thread_t* thread;
    int counter = 0;
    ppdb_error_t err;

    // Test thread creation
    err = ppdb_thread_create(&thread, thread_func, &counter);
    TEST_ASSERT(err == PPDB_OK, "Thread creation failed");

    // Test thread join
    err = ppdb_thread_join(thread);
    TEST_ASSERT(err == PPDB_OK, "Thread join failed");
    TEST_ASSERT(counter == 1, "Thread function did not execute");

    // Test thread destroy
    err = ppdb_thread_destroy(thread);
    TEST_ASSERT(err == PPDB_OK, "Thread destroy failed");

    return 0;
}

static int test_mutex(void) {
    ppdb_mutex_t* mutex;
    ppdb_error_t err;

    // Test mutex creation
    err = ppdb_mutex_create(&mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex creation failed");

    // Test mutex lock/unlock
    err = ppdb_mutex_lock(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex lock failed");

    err = ppdb_mutex_unlock(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex unlock failed");

    // Test mutex trylock
    err = ppdb_mutex_trylock(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex trylock failed");

    err = ppdb_mutex_unlock(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex unlock after trylock failed");

    // Test mutex destroy
    err = ppdb_mutex_destroy(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex destroy failed");

    return 0;
}

static int test_cond(void) {
    ppdb_mutex_t* mutex;
    ppdb_cond_t* cond;
    ppdb_error_t err;

    // Test condition variable creation
    err = ppdb_mutex_create(&mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex creation failed");

    err = ppdb_cond_create(&cond);
    TEST_ASSERT(err == PPDB_OK, "Condition variable creation failed");

    // Test condition variable signal
    err = ppdb_mutex_lock(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex lock failed");

    err = ppdb_cond_signal(cond);
    TEST_ASSERT(err == PPDB_OK, "Condition variable signal failed");

    err = ppdb_mutex_unlock(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex unlock failed");

    // Test condition variable broadcast
    err = ppdb_mutex_lock(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex lock failed");

    err = ppdb_cond_broadcast(cond);
    TEST_ASSERT(err == PPDB_OK, "Condition variable broadcast failed");

    err = ppdb_mutex_unlock(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex unlock failed");

    // Test condition variable destroy
    err = ppdb_cond_destroy(cond);
    TEST_ASSERT(err == PPDB_OK, "Condition variable destroy failed");

    err = ppdb_mutex_destroy(mutex);
    TEST_ASSERT(err == PPDB_OK, "Mutex destroy failed");

    return 0;
}

static int test_rwlock(void) {
    ppdb_rwlock_t* rwlock;
    ppdb_error_t err;

    // Test rwlock creation
    err = ppdb_rwlock_create(&rwlock);
    TEST_ASSERT(err == PPDB_OK, "RWLock creation failed");

    // Test read lock/unlock
    err = ppdb_rwlock_rdlock(rwlock);
    TEST_ASSERT(err == PPDB_OK, "RWLock read lock failed");

    err = ppdb_rwlock_unlock(rwlock);
    TEST_ASSERT(err == PPDB_OK, "RWLock read unlock failed");

    // Test write lock/unlock
    err = ppdb_rwlock_wrlock(rwlock);
    TEST_ASSERT(err == PPDB_OK, "RWLock write lock failed");

    err = ppdb_rwlock_unlock(rwlock);
    TEST_ASSERT(err == PPDB_OK, "RWLock write unlock failed");

    // Test rwlock destroy
    err = ppdb_rwlock_destroy(rwlock);
    TEST_ASSERT(err == PPDB_OK, "RWLock destroy failed");

    return 0;
}

int main(void) {
    int result = 0;

    printf("Running thread tests...\n");
    result = test_thread();
    if (result != 0) {
        return result;
    }
    printf("Thread tests passed.\n");

    printf("Running mutex tests...\n");
    result = test_mutex();
    if (result != 0) {
        return result;
    }
    printf("Mutex tests passed.\n");

    printf("Running condition variable tests...\n");
    result = test_cond();
    if (result != 0) {
        return result;
    }
    printf("Condition variable tests passed.\n");

    printf("Running read-write lock tests...\n");
    result = test_rwlock();
    if (result != 0) {
        return result;
    }
    printf("Read-write lock tests passed.\n");

    return 0;
}
