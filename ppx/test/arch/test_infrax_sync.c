#include "cosmopolitan.h"
#include "libc/thread/thread.h"
#include "libc/thread/semaphore.h"
#include "libc/atomic.h"
#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"

// Forward declarations
static InfraxCore* core = NULL;

// Helper function for memory management
InfraxMemory* get_memory_manager(void) {
    static InfraxMemory* memory = NULL;
    if (!memory) {
        InfraxMemoryConfig config = {
            .initial_size = 1024 * 1024,  // 1MB
            .use_gc = false,
            .use_pool = true,
            .gc_threshold = 0
        };
        memory = InfraxMemoryClass.new(&config);
    }
    return memory;
}

// Test functions
static void test_mutex(void) {
    if (!core) core = InfraxCoreClass.singleton();

    InfraxSync* mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    if (mutex == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "mutex != NULL", "Failed to create mutex");
    }

    // Test basic locking
    InfraxError err = mutex->mutex_lock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test unlocking
    err = mutex->mutex_unlock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test try_lock
    err = mutex->mutex_try_lock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = mutex->mutex_unlock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(mutex);
}

static void test_cond(void) {
    if (!core) core = InfraxCoreClass.singleton();

    // Create new mutex and condition variable instances
    InfraxSync* mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    InfraxSync* cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    if (mutex == NULL || cond == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "mutex != NULL && cond != NULL", "Failed to create mutex or condition");
    }

    // First lock the mutex
    InfraxError err = mutex->mutex_lock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test signal and broadcast
    err = cond->cond_signal(cond);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = cond->cond_broadcast(cond);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test wait with timeout
    err = cond->cond_timedwait(cond, mutex, 100);
    if (err.code != INFRAX_ERROR_SYNC_TIMEOUT) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == INFRAX_ERROR_SYNC_TIMEOUT", err.message);
    }

    // Unlock the mutex
    err = mutex->mutex_unlock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(mutex);
    InfraxSyncClass.free(cond);
}

static void test_rwlock(void) {
    if (!core) core = InfraxCoreClass.singleton();

    InfraxSync* rwlock = InfraxSyncClass.new(INFRAX_SYNC_TYPE_RWLOCK);
    if (rwlock == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "rwlock != NULL", "Failed to create rwlock");
    }

    // Test read locking
    InfraxError err = rwlock->rwlock_read_lock(rwlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = rwlock->rwlock_read_unlock(rwlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test write locking
    err = rwlock->rwlock_write_lock(rwlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = rwlock->rwlock_write_unlock(rwlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(rwlock);
}

static void test_spinlock(void) {
    if (!core) core = InfraxCoreClass.singleton();

    InfraxSync* spinlock = InfraxSyncClass.new(INFRAX_SYNC_TYPE_SPINLOCK);
    if (spinlock == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "spinlock != NULL", "Failed to create spinlock");
    }

    // Test basic locking
    InfraxError err = spinlock->spinlock_lock(spinlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = spinlock->spinlock_unlock(spinlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(spinlock);
}

static void test_semaphore(void) {
    if (!core) core = InfraxCoreClass.singleton();

    InfraxSync* sem = InfraxSyncClass.new(INFRAX_SYNC_TYPE_SEMAPHORE);
    if (sem == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "sem != NULL", "Failed to create semaphore");
    }

    int value;
    
    // Test get value
    InfraxError err = sem->semaphore_get_value(sem, &value);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    if (value != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "value == 0", "Initial semaphore value should be 0");
    }

    // Test post
    err = sem->semaphore_post(sem);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test get value after post
    err = sem->semaphore_get_value(sem, &value);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    if (value != 1) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "value == 1", "Semaphore value should be 1 after post");
    }

    // Test wait
    err = sem->semaphore_wait(sem);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(sem);
}

static void test_atomic(void) {
    if (!core) core = InfraxCoreClass.singleton();

    InfraxSync* atomic = InfraxSyncClass.new(INFRAX_SYNC_TYPE_ATOMIC);
    if (atomic == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic != NULL", "Failed to create atomic");
    }

    // Test atomic operations
    atomic_store(&atomic->value, 42);
    if (atomic_load(&atomic->value) != 42) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(&atomic->value) == 42", "Atomic store/load failed");
    }

    int64_t old_value = atomic_exchange(&atomic->value, 100);
    if (old_value != 42) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == 42", "Atomic exchange failed");
    }
    if (atomic_load(&atomic->value) != 100) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(&atomic->value) == 100", "Atomic exchange failed");
    }

    old_value = atomic_fetch_add(&atomic->value, 10);
    if (old_value != 100) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == 100", "Atomic fetch_add failed");
    }
    if (atomic_load(&atomic->value) != 110) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(&atomic->value) == 110", "Atomic fetch_add failed");
    }

    old_value = atomic_fetch_sub(&atomic->value, 10);
    if (old_value != 110) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == 110", "Atomic fetch_sub failed");
    }
    if (atomic_load(&atomic->value) != 100) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(&atomic->value) == 100", "Atomic fetch_sub failed");
    }

    old_value = atomic_fetch_and(&atomic->value, 0xFF);
    if (old_value != 100) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == 100", "Atomic fetch_and failed");
    }
    if (atomic_load(&atomic->value) != (100 & 0xFF)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(&atomic->value) == (100 & 0xFF)", "Atomic fetch_and failed");
    }

    old_value = atomic_fetch_or(&atomic->value, 0xF0);
    if (old_value != (100 & 0xFF)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == (100 & 0xFF)", "Atomic fetch_or failed");
    }
    if (atomic_load(&atomic->value) != ((100 & 0xFF) | 0xF0)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(&atomic->value) == ((100 & 0xFF) | 0xF0)", "Atomic fetch_or failed");
    }

    old_value = atomic_fetch_xor(&atomic->value, 0xFF);
    if (old_value != ((100 & 0xFF) | 0xF0)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == ((100 & 0xFF) | 0xF0)", "Atomic fetch_xor failed");
    }
    if (atomic_load(&atomic->value) != (((100 & 0xFF) | 0xF0) ^ 0xFF)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(&atomic->value) == (((100 & 0xFF) | 0xF0) ^ 0xFF)", "Atomic fetch_xor failed");
    }

    // Clean up
    InfraxSyncClass.free(atomic);
}

int main() {
    printf("===================\nStarting InfraxSync tests...\n");
    
    test_mutex();
    printf("Mutex test passed\n");
    
    test_cond();
    printf("Condition variable test passed\n");
    
    test_rwlock();
    printf("RWLock test passed\n");
    
    test_spinlock();
    printf("Spinlock test passed\n");
    
    test_semaphore();
    printf("Semaphore test passed\n");
    
    test_atomic();
    printf("Atomic test passed\n");
    
    printf("All InfraxSync tests passed!\n");
    return 0;
}