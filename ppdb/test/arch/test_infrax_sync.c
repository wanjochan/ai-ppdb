#include "cosmopolitan.h"
#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxCore.h"
#include <assert.h>

static void test_mutex(void) {
    InfraxSync* mutex = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_MUTEX);
    assert(mutex != NULL);

    // Test basic locking
    assert(mutex->mutex_lock(mutex).code == INFRAX_ERROR_OK);
    assert(mutex->mutex_lock(mutex).code == INFRAX_ERROR_OK);  // Should succeed because it's a recursive mutex
    assert(mutex->mutex_unlock(mutex).code == INFRAX_ERROR_OK);
    assert(mutex->mutex_unlock(mutex).code == INFRAX_ERROR_OK);
    assert(mutex->mutex_try_lock(mutex).code == INFRAX_ERROR_OK);
    assert(mutex->mutex_unlock(mutex).code == INFRAX_ERROR_OK);

    // Test try lock when mutex is already locked
    assert(mutex->mutex_lock(mutex).code == INFRAX_ERROR_OK);
    assert(mutex->mutex_try_lock(mutex).code == INFRAX_ERROR_OK);  // Should succeed because it's a recursive mutex
    assert(mutex->mutex_unlock(mutex).code == INFRAX_ERROR_OK);
    assert(mutex->mutex_unlock(mutex).code == INFRAX_ERROR_OK);

    InfraxSync_CLASS.free(mutex);
}

static void test_cond(void) {
    InfraxSync* mutex = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_MUTEX);
    InfraxSync* cond = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_CONDITION);
    assert(mutex != NULL && cond != NULL);

    // Test signal and broadcast
    assert(cond->cond_signal(cond).code == INFRAX_ERROR_OK);
    assert(cond->cond_broadcast(cond).code == INFRAX_ERROR_OK);

    // Test timed wait
    assert(mutex->mutex_lock(mutex).code == INFRAX_ERROR_OK);
    assert(cond->cond_timedwait(cond, mutex, 100).code == INFRAX_ERROR_SYNC_TIMEOUT);
    assert(mutex->mutex_unlock(mutex).code == INFRAX_ERROR_OK);

    InfraxSync_CLASS.free(cond);
    InfraxSync_CLASS.free(mutex);
}

static void test_rwlock(void) {
    InfraxSync* rwlock = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_RWLOCK);
    assert(rwlock != NULL);

    // Test read locking
    assert(rwlock->rwlock_read_lock(rwlock).code == INFRAX_ERROR_OK);
    assert(rwlock->rwlock_read_lock(rwlock).code == INFRAX_ERROR_OK); // Multiple readers allowed
    assert(rwlock->rwlock_try_write_lock(rwlock).code == INFRAX_ERROR_SYNC_WOULD_BLOCK);
    assert(rwlock->rwlock_read_unlock(rwlock).code == INFRAX_ERROR_OK);
    assert(rwlock->rwlock_read_unlock(rwlock).code == INFRAX_ERROR_OK);

    // Test write locking
    assert(rwlock->rwlock_write_lock(rwlock).code == INFRAX_ERROR_OK);
    assert(rwlock->rwlock_try_read_lock(rwlock).code == INFRAX_ERROR_SYNC_WOULD_BLOCK);
    assert(rwlock->rwlock_try_write_lock(rwlock).code == INFRAX_ERROR_SYNC_WOULD_BLOCK);
    assert(rwlock->rwlock_write_unlock(rwlock).code == INFRAX_ERROR_OK);

    InfraxSync_CLASS.free(rwlock);
}

static void test_spinlock(void) {
    InfraxSync* spinlock = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_SPINLOCK);
    assert(spinlock != NULL);

    // Test basic locking
    assert(spinlock->spinlock_lock(spinlock).code == INFRAX_ERROR_OK);
    assert(spinlock->spinlock_try_lock(spinlock).code == INFRAX_ERROR_SYNC_WOULD_BLOCK);
    assert(spinlock->spinlock_unlock(spinlock).code == INFRAX_ERROR_OK);
    assert(spinlock->spinlock_try_lock(spinlock).code == INFRAX_ERROR_OK);
    assert(spinlock->spinlock_unlock(spinlock).code == INFRAX_ERROR_OK);

    InfraxSync_CLASS.free(spinlock);
}

static void test_semaphore(void) {
    InfraxSync* sem = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_SEMAPHORE);
    assert(sem != NULL);

    int value;
    // Test initial value
    assert(sem->semaphore_get_value(sem, &value).code == INFRAX_ERROR_OK);
    assert(value == 1);

    // Test wait and post
    assert(sem->semaphore_wait(sem).code == INFRAX_ERROR_OK);
    assert(sem->semaphore_get_value(sem, &value).code == INFRAX_ERROR_OK);
    assert(value == 0);

    assert(sem->semaphore_try_wait(sem).code == INFRAX_ERROR_SYNC_WOULD_BLOCK);
    assert(sem->semaphore_post(sem).code == INFRAX_ERROR_OK);
    assert(sem->semaphore_get_value(sem, &value).code == INFRAX_ERROR_OK);
    assert(value == 1);

    InfraxSync_CLASS.free(sem);
}

static void test_atomic(void) {
    InfraxSync* atomic = InfraxSync_CLASS.new(INFRAX_SYNC_TYPE_ATOMIC);
    assert(atomic != NULL);

    // Test atomic operations
    (*atomic->atomic_store)(atomic, 10);
    assert((*atomic->atomic_load)(atomic) == 10);

    (*atomic->atomic_store)(atomic, 20);
    assert((*atomic->atomic_load)(atomic) == 20);

    assert((*atomic->atomic_fetch_add)(atomic, 1) == 20);
    assert((*atomic->atomic_load)(atomic) == 21);

    assert((*atomic->atomic_fetch_sub)(atomic, 1) == 21);
    assert((*atomic->atomic_load)(atomic) == 20);

    assert((*atomic->atomic_fetch_add)(atomic, 5) == 20);
    assert((*atomic->atomic_load)(atomic) == 25);

    assert((*atomic->atomic_fetch_sub)(atomic, 15) == 25);
    assert((*atomic->atomic_load)(atomic) == 10);

    // Test atomic exchange
    assert((*atomic->atomic_exchange)(atomic, 30) == 10);
    assert((*atomic->atomic_load)(atomic) == 30);

    // Test atomic compare exchange
    int64_t expected = 30;
    assert((*atomic->atomic_compare_exchange)(atomic, &expected, 40));
    assert((*atomic->atomic_load)(atomic) == 40);

    // Test atomic bitwise operations
    (*atomic->atomic_store)(atomic, 0xFF);
    assert((*atomic->atomic_fetch_and)(atomic, 0xF0) == 0xFF);
    assert((*atomic->atomic_load)(atomic) == 0xF0);

    assert((*atomic->atomic_fetch_or)(atomic, 0x0F) == 0xF0);
    assert((*atomic->atomic_load)(atomic) == 0xFF);

    assert((*atomic->atomic_fetch_xor)(atomic, 0xFF) == 0xFF);
    assert((*atomic->atomic_load)(atomic) == 0);

    InfraxSync_CLASS.free(atomic);
}

int main(void) {
    printf("Running synchronization tests...\n");

    test_mutex();
    printf("Mutex tests passed\n");

    test_cond();
    printf("Condition variable tests passed\n");

    test_rwlock();
    printf("Read-write lock tests passed\n");

    test_spinlock();
    printf("Spinlock tests passed\n");

    test_semaphore();
    printf("Semaphore tests passed\n");

    test_atomic();
    printf("Atomic operations tests passed\n");

    printf("All synchronization tests passed!\n");
    return 0;
}