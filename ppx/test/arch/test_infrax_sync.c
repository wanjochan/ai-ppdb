#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"

// Forward declarations
InfraxCore* core = NULL;

// Helper function for memory management
InfraxMemory* get_memory_manager(void) {
    static InfraxMemory* memory = NULL;
    if (!memory) {
        InfraxMemoryConfig config = {
            .initial_size = 1024 * 1024,  // 1MB
            .use_gc = INFRAX_FALSE,
            .use_pool = INFRAX_TRUE,
            .gc_threshold = 0
        };
        memory = InfraxMemoryClass.new(&config);
    }
    return memory;
}

// Test functions
static void test_mutex(void) {

    InfraxSync* mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    if (mutex == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "mutex != NULL", "Failed to create mutex");
    }

    // Test basic locking
    InfraxError err = mutex->klass->mutex_lock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test unlocking
    err = mutex->klass->mutex_unlock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test try_lock
    err = mutex->klass->mutex_try_lock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = mutex->klass->mutex_unlock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(mutex);
}

static void test_cond(void) {

    // Create new mutex and condition variable instances
    InfraxSync* mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    InfraxSync* cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    if (mutex == NULL || cond == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "mutex != NULL && cond != NULL", "Failed to create mutex or condition");
    }

    // First lock the mutex
    InfraxError err = mutex->klass->mutex_lock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test signal and broadcast
    err = cond->klass->cond_signal(cond);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = cond->klass->cond_broadcast(cond);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test wait with timeout
    err = cond->klass->cond_timedwait(cond, mutex, 100);
    if (err.code != INFRAX_ERROR_SYNC_TIMEOUT) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == INFRAX_ERROR_SYNC_TIMEOUT", err.message);
    }

    // Unlock the mutex
    err = mutex->klass->mutex_unlock(mutex);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(mutex);
    InfraxSyncClass.free(cond);
}

static void test_rwlock(void) {

    InfraxSync* rwlock = InfraxSyncClass.new(INFRAX_SYNC_TYPE_RWLOCK);
    if (rwlock == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "rwlock != NULL", "Failed to create rwlock");
    }

    // Test read locking
    InfraxError err = rwlock->klass->rwlock_read_lock(rwlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = rwlock->klass->rwlock_read_unlock(rwlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test write locking
    err = rwlock->klass->rwlock_write_lock(rwlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = rwlock->klass->rwlock_write_unlock(rwlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(rwlock);
}

static void test_spinlock(void) {

    InfraxSync* spinlock = InfraxSyncClass.new(INFRAX_SYNC_TYPE_SPINLOCK);
    if (spinlock == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "spinlock != NULL", "Failed to create spinlock");
    }

    // Test basic locking
    InfraxError err = spinlock->klass->spinlock_lock(spinlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    err = spinlock->klass->spinlock_unlock(spinlock);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(spinlock);
}

static void test_semaphore(void) {

    InfraxSync* sem = InfraxSyncClass.new(INFRAX_SYNC_TYPE_SEMAPHORE);
    if (sem == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "sem != NULL", "Failed to create semaphore");
    }

    int value;
    
    // Test get value
    InfraxError err = sem->klass->semaphore_get_value(sem, &value);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    if (value != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "value == 0", "Initial semaphore value should be 0");
    }

    // Test post
    err = sem->klass->semaphore_post(sem);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Test get value after post
    err = sem->klass->semaphore_get_value(sem, &value);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }
    if (value != 1) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "value == 1", "Semaphore value should be 1 after post");
    }

    // Test wait
    err = sem->klass->semaphore_wait(sem);
    if (err.code != 0) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "err.code == 0", err.message);
    }

    // Clean up
    InfraxSyncClass.free(sem);
}

static void test_atomic(void) {
    InfraxSync* atomic = InfraxSyncClass.new(INFRAX_SYNC_TYPE_ATOMIC);
    if (atomic == NULL) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic != NULL", "Failed to create atomic");
    }

    // Test atomic operations
    atomic->klass->atomic_store(atomic, 42);
    if (atomic->klass->atomic_load(atomic) != 42) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(atomic) == 42", "Atomic store/load failed");
    }

    InfraxI64 old_value = atomic->klass->atomic_exchange(atomic, 42);
    if (atomic->klass->atomic_load(atomic) != 42) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(atomic) == 42", "Atomic store/load failed");
    }

    old_value = atomic->klass->atomic_exchange(atomic, 100);
    if (old_value != 42) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == 42", "Atomic exchange failed");
    }
    if (atomic->klass->atomic_load(atomic) != 100) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(atomic) == 100", "Atomic exchange failed");
    }

    old_value = atomic->klass->atomic_fetch_add(atomic, 10);
    if (old_value != 100) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == 100", "Atomic fetch_add failed");
    }
    if (atomic->klass->atomic_load(atomic) != 110) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(atomic) == 110", "Atomic fetch_add failed");
    }

    old_value = atomic->klass->atomic_fetch_sub(atomic, 10);
    if (old_value != 110) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == 110", "Atomic fetch_sub failed");
    }
    if (atomic->klass->atomic_load(atomic) != 100) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(atomic) == 100", "Atomic fetch_sub failed");
    }

    old_value = atomic->klass->atomic_fetch_and(atomic, 0xFF);
    if (old_value != 100) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == 100", "Atomic fetch_and failed");
    }
    if (atomic->klass->atomic_load(atomic) != (100 & 0xFF)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(atomic) == (100 & 0xFF)", "Atomic fetch_and failed");
    }

    old_value = atomic->klass->atomic_fetch_or(atomic, 0xF0);
    if (old_value != (100 & 0xFF)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == (100 & 0xFF)", "Atomic fetch_or failed");
    }
    if (atomic->klass->atomic_load(atomic) != ((100 & 0xFF) | 0xF0)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(atomic) == ((100 & 0xFF) | 0xF0)", "Atomic fetch_or failed");
    }

    old_value = atomic->klass->atomic_fetch_xor(atomic, 0xFF);
    if (old_value != ((100 & 0xFF) | 0xF0)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "old_value == ((100 & 0xFF) | 0xF0)", "Atomic fetch_xor failed");
    }
    if (atomic->klass->atomic_load(atomic) != (((100 & 0xFF) | 0xF0) ^ 0xFF)) {
        core->assert_failed(core, __FILE__, __LINE__, __func__, "atomic_load(atomic) == (((100 & 0xFF) | 0xF0) ^ 0xFF)", "Atomic fetch_xor failed");
    }

    // Clean up
    InfraxSyncClass.free(atomic);
}

// 添加新的测试函数声明
void test_sync_stress(void);
void test_deadlock_detection(void);
void test_condition_variable_detailed(void);
void test_rwlock_fairness(void);
void test_semaphore_edge_cases(void);

// 添加新的测试函数实现
void test_sync_stress() {
    core->printf(core, "Testing synchronization stress...\n");
    
    #define NUM_ITERATIONS 10000
    #define SHARED_VALUE_TARGET (NUM_ITERATIONS * 2)
    
    // 共享资源
    int64_t shared_value = 0;
    
    // 创建同步原语
    InfraxSync* mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    InfraxSync* atomic = InfraxSyncClass.new(INFRAX_SYNC_TYPE_ATOMIC);
    INFRAX_ASSERT(core, mutex != NULL && atomic != NULL);
    
    // 1. 互斥锁压力测试
    for(int i = 0; i < NUM_ITERATIONS; i++) {
        // 模拟两个线程交替访问
        InfraxError err = mutex->klass->mutex_lock(mutex);
        INFRAX_ASSERT(core, err.code == 0);
        shared_value++;
        err = mutex->klass->mutex_unlock(mutex);
        INFRAX_ASSERT(core, err.code == 0);
        
        err = mutex->klass->mutex_lock(mutex);
        INFRAX_ASSERT(core, err.code == 0);
        shared_value++;
        err = mutex->klass->mutex_unlock(mutex);
        INFRAX_ASSERT(core, err.code == 0);
    }
    
    INFRAX_ASSERT(core, shared_value == SHARED_VALUE_TARGET);
    
    // 2. 原子操作压力测试
    atomic->klass->atomic_store(atomic, 0);
    for(int i = 0; i < NUM_ITERATIONS; i++) {
        atomic->klass->atomic_fetch_add(atomic, 1);
        atomic->klass->atomic_fetch_add(atomic, 1);
    }
    
    INFRAX_ASSERT(core, atomic->klass->atomic_load(atomic) == SHARED_VALUE_TARGET);
    
    InfraxSyncClass.free(mutex);
    InfraxSyncClass.free(atomic);
    
    core->printf(core, "Synchronization stress test passed\n");
}

void test_deadlock_detection() {
    core->printf(core, "Testing deadlock detection...\n");
    
    InfraxSync* mutex1 = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    InfraxSync* mutex2 = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    INFRAX_ASSERT(core, mutex1 != NULL && mutex2 != NULL);
    
    // 测试嵌套锁定（应该检测到潜在死锁）
    InfraxError err = mutex1->klass->mutex_lock(mutex1);
    INFRAX_ASSERT(core, err.code == 0);
    
    // 尝试锁定第二个互斥锁，应该超时而不是死锁
    err = mutex2->klass->mutex_try_lock(mutex2);
    if(err.code == 0) {
        mutex2->klass->mutex_unlock(mutex2);
    }
    
    mutex1->klass->mutex_unlock(mutex1);
    
    InfraxSyncClass.free(mutex1);
    InfraxSyncClass.free(mutex2);
    
    core->printf(core, "Deadlock detection test passed\n");
}

void test_condition_variable_detailed() {
    core->printf(core, "Testing condition variable details...\n");
    
    InfraxSync* mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    InfraxSync* cond = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    INFRAX_ASSERT(core, mutex != NULL && cond != NULL);
    
    // 1. 测试不同的超时值
    InfraxError err = mutex->klass->mutex_lock(mutex);
    INFRAX_ASSERT(core, err.code == 0);
    
    // 短超时
    err = cond->klass->cond_timedwait(cond, mutex, 1);  // 1ms
    INFRAX_ASSERT(core, err.code == INFRAX_ERROR_SYNC_TIMEOUT);
    
    // 长超时
    err = cond->klass->cond_timedwait(cond, mutex, 100);  // 100ms
    INFRAX_ASSERT(core, err.code == INFRAX_ERROR_SYNC_TIMEOUT);
    
    mutex->klass->mutex_unlock(mutex);
    
    // 2. 测试虚假唤醒处理
    InfraxBool condition_met = INFRAX_FALSE;
    int spurious_wakeup_count = 0;
    
    err = mutex->klass->mutex_lock(mutex);
    INFRAX_ASSERT(core, err.code == 0);
    
    while(!condition_met && spurious_wakeup_count < 3) {
        err = cond->klass->cond_timedwait(cond, mutex, 10);
        if(err.code == INFRAX_ERROR_SYNC_TIMEOUT) {
            spurious_wakeup_count++;
        }
    }
    
    mutex->klass->mutex_unlock(mutex);
    
    InfraxSyncClass.free(mutex);
    InfraxSyncClass.free(cond);
    
    core->printf(core, "Condition variable detail test passed\n");
}

void test_rwlock_fairness() {
    core->printf(core, "Testing read-write lock fairness...\n");
    
    InfraxSync* rwlock = InfraxSyncClass.new(INFRAX_SYNC_TYPE_RWLOCK);
    INFRAX_ASSERT(core, rwlock != NULL);
    
    // 1. 测试读优先
    for(int i = 0; i < 100; i++) {
        InfraxError err = rwlock->klass->rwlock_read_lock(rwlock);
        INFRAX_ASSERT(core, err.code == 0);
        
        // 模拟读操作
        core->sleep_ms(core, 1);
        
        err = rwlock->klass->rwlock_read_unlock(rwlock);
        INFRAX_ASSERT(core, err.code == 0);
    }
    
    // 2. 测试写优先
    for(int i = 0; i < 10; i++) {
        InfraxError err = rwlock->klass->rwlock_write_lock(rwlock);
        INFRAX_ASSERT(core, err.code == 0);
        
        // 模拟写操作
        core->sleep_ms(core, 5);
        
        err = rwlock->klass->rwlock_write_unlock(rwlock);
        INFRAX_ASSERT(core, err.code == 0);
    }
    
    // 3. 测试读写交替
    for(int i = 0; i < 10; i++) {
        InfraxError err = rwlock->klass->rwlock_read_lock(rwlock);
        INFRAX_ASSERT(core, err.code == 0);
        
        core->sleep_ms(core, 1);
        
        err = rwlock->klass->rwlock_read_unlock(rwlock);
        INFRAX_ASSERT(core, err.code == 0);
        
        err = rwlock->klass->rwlock_write_lock(rwlock);
        INFRAX_ASSERT(core, err.code == 0);
        
        core->sleep_ms(core, 1);
        
        err = rwlock->klass->rwlock_write_unlock(rwlock);
        INFRAX_ASSERT(core, err.code == 0);
    }
    
    InfraxSyncClass.free(rwlock);
    
    core->printf(core, "Read-write lock fairness test passed\n");
}

void test_semaphore_edge_cases() {
    core->printf(core, "Testing semaphore edge cases...\n");
    
    InfraxSync* sem = InfraxSyncClass.new(INFRAX_SYNC_TYPE_SEMAPHORE);
    INFRAX_ASSERT(core, sem != NULL);
    
    int value;
    
    // 1. 测试最大值
    for(int i = 0; i < 1000; i++) {
        InfraxError err = sem->klass->semaphore_post(sem);
        INFRAX_ASSERT(core, err.code == 0);
    }
    
    InfraxError err = sem->klass->semaphore_get_value(sem, &value);
    INFRAX_ASSERT(core, err.code == 0);
    INFRAX_ASSERT(core, value == 1000);
    
    // 2. 测试快速post/wait
    for(int i = 0; i < 1000; i++) {
        err = sem->klass->semaphore_wait(sem);
        INFRAX_ASSERT(core, err.code == 0);
    }
    
    err = sem->klass->semaphore_get_value(sem, &value);
    INFRAX_ASSERT(core, err.code == 0);
    INFRAX_ASSERT(core, value == 0);
    
    // 3. 测试超时等待
    err = sem->klass->semaphore_try_wait(sem);  // 尝试等待
    INFRAX_ASSERT(core, err.code == INFRAX_ERROR_SYNC_WOULD_BLOCK);
    
    InfraxSyncClass.free(sem);
    
    core->printf(core, "Semaphore edge cases test passed\n");
}

int main() {
    core = InfraxCoreClass.singleton();
    INFRAX_ASSERT(core, core != NULL);
    
    core->printf(core, "===================\n");
    core->printf(core, "Starting InfraxSync tests...\n");
    
    // 基本测试
    test_mutex();
    test_cond();
    test_rwlock();
    test_spinlock();
    test_semaphore();
    test_atomic();
    
    // 新增的高级测试
    test_sync_stress();
    test_deadlock_detection();
    test_condition_variable_detailed();
    test_rwlock_fairness();
    test_semaphore_edge_cases();
    
    core->printf(core, "All infrax_sync tests passed!\n");
    core->printf(core, "===================\n");
    
    return 0;
}