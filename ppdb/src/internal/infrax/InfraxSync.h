#ifndef INFRAX_SYNC_H
#define INFRAX_SYNC_H
/* 同步原证包括
Mutex (互斥锁)
RWLock (读写锁)
Spinlock (自旋锁)
Semaphore (信号量)
Condition (条件变量)
Atomic (原子操作)
*/
#include <stdbool.h>
#include <pthread.h>
#include "InfraxCore.h"

// Error codes
#define INFRAX_ERROR_SYNC_INVALID_ARGUMENT -1
#define INFRAX_ERROR_SYNC_INIT_FAILED -2
#define INFRAX_ERROR_SYNC_LOCK_FAILED -3
#define INFRAX_ERROR_SYNC_UNLOCK_FAILED -4
#define INFRAX_ERROR_SYNC_WAIT_FAILED -5
#define INFRAX_ERROR_SYNC_SIGNAL_FAILED -6
#define INFRAX_ERROR_SYNC_TIMEOUT -7

//-----------------------------------------------------------------------------
// Mutex
//-----------------------------------------------------------------------------
typedef struct InfraxMutex InfraxMutex;
typedef struct InfraxMutexClass InfraxMutexClass;

struct InfraxMutexClass {
    InfraxMutex* (*new)(void);
    void (*free)(InfraxMutex* self);
};

struct InfraxMutex {
    const InfraxMutexClass* klass;
    pthread_mutex_t native_handle;
    bool is_initialized;

    // Instance methods
    InfraxError (*lock)(InfraxMutex* self);
    InfraxError (*try_lock)(InfraxMutex* self);
    InfraxError (*unlock)(InfraxMutex* self);
};

//-----------------------------------------------------------------------------
// RWLock
//-----------------------------------------------------------------------------
typedef struct InfraxRWLock InfraxRWLock;
typedef struct InfraxRWLockClass InfraxRWLockClass;

struct InfraxRWLockClass {
    InfraxRWLock* (*new)(void);
    void (*free)(InfraxRWLock* self);
};

struct InfraxRWLock {
    const InfraxRWLockClass* klass;
    pthread_rwlock_t native_handle;
    bool is_initialized;

    // Instance methods
    InfraxError (*read_lock)(InfraxRWLock* self);
    InfraxError (*try_read_lock)(InfraxRWLock* self);
    InfraxError (*read_unlock)(InfraxRWLock* self);
    InfraxError (*write_lock)(InfraxRWLock* self);
    InfraxError (*try_write_lock)(InfraxRWLock* self);
    InfraxError (*write_unlock)(InfraxRWLock* self);
};

//-----------------------------------------------------------------------------
// Spinlock
//-----------------------------------------------------------------------------
typedef struct InfraxSpinlock InfraxSpinlock;
typedef struct InfraxSpinlockClass InfraxSpinlockClass;

struct InfraxSpinlockClass {
    InfraxSpinlock* (*new)(void);
    void (*free)(InfraxSpinlock* self);
};

struct InfraxSpinlock {
    const InfraxSpinlockClass* klass;
    pthread_spinlock_t native_handle;
    bool is_initialized;

    // Instance methods
    InfraxError (*lock)(InfraxSpinlock* self);
    InfraxError (*try_lock)(InfraxSpinlock* self);
    InfraxError (*unlock)(InfraxSpinlock* self);
};

//-----------------------------------------------------------------------------
// Semaphore
//-----------------------------------------------------------------------------
typedef struct InfraxSemaphore InfraxSemaphore;
typedef struct InfraxSemaphoreClass InfraxSemaphoreClass;

struct InfraxSemaphoreClass {
    InfraxSemaphore* (*new)(unsigned int initial_value);
    void (*free)(InfraxSemaphore* self);
};

struct InfraxSemaphore {
    const InfraxSemaphoreClass* klass;
    sem_t native_handle;
    bool is_initialized;

    // Instance methods
    InfraxError (*wait)(InfraxSemaphore* self);
    InfraxError (*try_wait)(InfraxSemaphore* self);
    InfraxError (*post)(InfraxSemaphore* self);
    InfraxError (*get_value)(InfraxSemaphore* self, int* value);
};

//-----------------------------------------------------------------------------
// Condition Variable
//-----------------------------------------------------------------------------
typedef struct InfraxCond InfraxCond;
typedef struct InfraxCondClass InfraxCondClass;

struct InfraxCondClass {
    InfraxCond* (*new)(void);
    void (*free)(InfraxCond* self);
};

struct InfraxCond {
    const InfraxCondClass* klass;
    pthread_cond_t native_handle;
    bool is_initialized;

    // Instance methods
    InfraxError (*wait)(InfraxCond* self, InfraxMutex* mutex);
    InfraxError (*timedwait)(InfraxCond* self, InfraxMutex* mutex, InfraxTime timeout_ms);
    InfraxError (*signal)(InfraxCond* self);
    InfraxError (*broadcast)(InfraxCond* self);
};

//-----------------------------------------------------------------------------
// Atomic Operations
//-----------------------------------------------------------------------------
typedef struct InfraxAtomic InfraxAtomic;
typedef struct InfraxAtomicClass InfraxAtomicClass;

struct InfraxAtomicClass {
    InfraxAtomic* (*new)(void);
    void (*free)(InfraxAtomic* self);
};

struct InfraxAtomic {
    const InfraxAtomicClass* klass;
    _Atomic int64_t value;

    // Instance methods
    int64_t (*load)(InfraxAtomic* self);
    void (*store)(InfraxAtomic* self, int64_t value);
    int64_t (*exchange)(InfraxAtomic* self, int64_t value);
    bool (*compare_exchange)(InfraxAtomic* self, int64_t* expected, int64_t desired);
    int64_t (*fetch_add)(InfraxAtomic* self, int64_t value);
    int64_t (*fetch_sub)(InfraxAtomic* self, int64_t value);
    int64_t (*fetch_and)(InfraxAtomic* self, int64_t value);
    int64_t (*fetch_or)(InfraxAtomic* self, int64_t value);
    int64_t (*fetch_xor)(InfraxAtomic* self, int64_t value);
};

// The "static" interface instances
extern const InfraxMutexClass InfraxMutex_CLASS;
extern const InfraxRWLockClass InfraxRWLock_CLASS;
extern const InfraxSpinlockClass InfraxSpinlock_CLASS;
extern const InfraxSemaphoreClass InfraxSemaphore_CLASS;
extern const InfraxCondClass InfraxCond_CLASS;
extern const InfraxAtomicClass InfraxAtomic_CLASS;

#endif // INFRAX_SYNC_H 