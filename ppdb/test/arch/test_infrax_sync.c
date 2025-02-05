#include "cosmopolitan.h"
#include <assert.h>
#include "internal/infrax/InfraxSync.h"

static void test_mutex(void) {
    printf("Testing mutex...\n");
    
    InfraxMutex mutex;
    assert(InfraxMutexCreate(&mutex).code == INFRAX_OK);
    assert(mutex != NULL);
    
    assert(InfraxMutexLock(mutex).code == INFRAX_OK);
    assert(InfraxMutexTryLock(mutex).code == INFRAX_ERROR_WOULD_BLOCK);
    assert(InfraxMutexUnlock(mutex).code == INFRAX_OK);
    assert(InfraxMutexTryLock(mutex).code == INFRAX_OK);
    assert(InfraxMutexUnlock(mutex).code == INFRAX_OK);
    
    InfraxMutexDestroy(mutex);
    printf("Mutex tests passed\n");
}

static void test_cond(void) {
    printf("Testing condition variable...\n");
    
    InfraxMutex mutex;
    InfraxCond cond;
    assert(InfraxMutexCreate(&mutex).code == INFRAX_OK);
    assert(InfraxCondCreate(&cond).code == INFRAX_OK);
    
    assert(InfraxCondSignal(cond).code == INFRAX_OK);
    assert(InfraxCondBroadcast(cond).code == INFRAX_OK);
    
    // Test timed wait
    assert(InfraxMutexLock(mutex).code == INFRAX_OK);
    assert(InfraxCondTimedWait(cond, mutex, 100).code == INFRAX_ERROR_SYNC_TIMEOUT);
    assert(InfraxMutexUnlock(mutex).code == INFRAX_OK);
    
    InfraxCondDestroy(cond);
    InfraxMutexDestroy(mutex);
    printf("Condition variable tests passed\n");
}

static void test_rwlock(void) {
    printf("Testing read-write lock...\n");
    
    InfraxRWLock rwlock;
    assert(InfraxRWLockCreate(&rwlock).code == INFRAX_OK);
    
    // Test read lock
    assert(InfraxRWLockRDLock(rwlock).code == INFRAX_OK);
    assert(InfraxRWLockRDLock(rwlock).code == INFRAX_OK); // Multiple readers allowed
    assert(InfraxRWLockTryWRLock(rwlock).code == INFRAX_ERROR_WOULD_BLOCK);
    assert(InfraxRWLockUnlock(rwlock).code == INFRAX_OK);
    assert(InfraxRWLockUnlock(rwlock).code == INFRAX_OK);
    
    // Test write lock
    assert(InfraxRWLockWRLock(rwlock).code == INFRAX_OK);
    assert(InfraxRWLockTryRDLock(rwlock).code == INFRAX_ERROR_WOULD_BLOCK);
    assert(InfraxRWLockTryWRLock(rwlock).code == INFRAX_ERROR_WOULD_BLOCK);
    assert(InfraxRWLockUnlock(rwlock).code == INFRAX_OK);
    
    InfraxRWLockDestroy(rwlock);
    printf("Read-write lock tests passed\n");
}

static void test_spinlock(void) {
    printf("Testing spinlock...\n");
    
    InfraxSpinLock spinlock;
    InfraxSpinLockInit(&spinlock);
    
    assert(InfraxSpinLockTryLock(&spinlock).code == INFRAX_OK);
    assert(InfraxSpinLockTryLock(&spinlock).code == INFRAX_ERROR_WOULD_BLOCK);
    InfraxSpinLockUnlock(&spinlock);
    assert(InfraxSpinLockTryLock(&spinlock).code == INFRAX_OK);
    InfraxSpinLockUnlock(&spinlock);
    
    InfraxSpinLockDestroy(&spinlock);
    printf("Spinlock tests passed\n");
}

static void test_semaphore(void) {
    printf("Testing semaphore...\n");
    
    InfraxSem sem;
    assert(InfraxSemCreate(&sem, 1).code == INFRAX_OK);
    
    int value;
    assert(InfraxSemGetValue(&sem, &value).code == INFRAX_OK);
    assert(value == 1);
    
    assert(InfraxSemWait(&sem).code == INFRAX_OK);
    assert(InfraxSemGetValue(&sem, &value).code == INFRAX_OK);
    assert(value == 0);
    
    assert(InfraxSemTryWait(&sem).code == INFRAX_ERROR_WOULD_BLOCK);
    assert(InfraxSemTimedWait(&sem, 100).code == INFRAX_ERROR_SYNC_TIMEOUT);
    
    assert(InfraxSemPost(&sem).code == INFRAX_OK);
    assert(InfraxSemGetValue(&sem, &value).code == INFRAX_OK);
    assert(value == 1);
    
    InfraxSemDestroy(&sem);
    printf("Semaphore tests passed\n");
}

static void test_atomic(void) {
    printf("Testing atomic operations...\n");
    
    InfraxAtomic atomic;
    InfraxAtomicInit(&atomic, 10);
    
    InfraxAtomicResult result;
    
    result = InfraxAtomicGet(&atomic);
    assert(result.code.code == INFRAX_OK);
    assert(result.value == 10);
    
    InfraxError err = InfraxAtomicSet(&atomic, 20);
    assert(err.code == INFRAX_OK);
    result = InfraxAtomicGet(&atomic);
    assert(result.code.code == INFRAX_OK);
    assert(result.value == 20);
    
    result = InfraxAtomicInc(&atomic);
    assert(result.code.code == INFRAX_OK);
    assert(result.value == 21);
    
    result = InfraxAtomicDec(&atomic);
    assert(result.code.code == INFRAX_OK);
    assert(result.value == 20);
    
    result = InfraxAtomicAdd(&atomic, 5);
    assert(result.code.code == INFRAX_OK);
    assert(result.value == 25);
    
    result = InfraxAtomicSub(&atomic, 15);
    assert(result.code.code == INFRAX_OK);
    assert(result.value == 10);
    
    printf("Atomic operations tests passed\n");
}

int main(void) {
    printf("Starting InfraxSync tests...\n\n");
    
    test_mutex();
    printf("\n");
    
    test_cond();
    printf("\n");
    
    test_rwlock();
    printf("\n");
    
    test_spinlock();
    printf("\n");
    
    test_semaphore();
    printf("\n");
    
    test_atomic();
    printf("\n");
    
    printf("All InfraxSync tests passed!\n");
    return 0;
} 