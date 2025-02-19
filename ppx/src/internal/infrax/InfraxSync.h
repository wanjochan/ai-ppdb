#ifndef INFRAX_SYNC_H
#define INFRAX_SYNC_H

//sync primitive
//design pattern: factory (by type)

#include "InfraxCore.h"

// Error codes
#define INFRAX_ERROR_SYNC_OK 0
#define INFRAX_ERROR_SYNC_INVALID_ARGUMENT -101
#define INFRAX_ERROR_SYNC_INIT_FAILED -102
#define INFRAX_ERROR_SYNC_LOCK_FAILED -103
#define INFRAX_ERROR_SYNC_UNLOCK_FAILED -104
#define INFRAX_ERROR_SYNC_WAIT_FAILED -105
#define INFRAX_ERROR_SYNC_SIGNAL_FAILED -106
#define INFRAX_ERROR_SYNC_TIMEOUT -107
#define INFRAX_ERROR_SYNC_WOULD_BLOCK -108

// Forward declarations
typedef struct InfraxSync InfraxSync;
typedef struct InfraxSyncClassType InfraxSyncClassType;

// Sync type
typedef enum {
    INFRAX_SYNC_TYPE_MUTEX,
    INFRAX_SYNC_TYPE_RWLOCK,
    INFRAX_SYNC_TYPE_SPINLOCK,
    INFRAX_SYNC_TYPE_SEMAPHORE,
    INFRAX_SYNC_TYPE_CONDITION,
    INFRAX_SYNC_TYPE_ATOMIC
} InfraxSyncType;

// The "static" interface (like static methods in OOP)
struct InfraxSyncClassType {
    InfraxSync* (*new)(InfraxSyncType type);
    void (*free)(InfraxSync* sync);

    // Instance methods moved from InfraxSync
    InfraxError (*mutex_lock)(InfraxSync* self);
    InfraxError (*mutex_try_lock)(InfraxSync* self);
    InfraxError (*mutex_unlock)(InfraxSync* self);

    InfraxError (*rwlock_read_lock)(InfraxSync* self);
    InfraxError (*rwlock_try_read_lock)(InfraxSync* self);
    InfraxError (*rwlock_read_unlock)(InfraxSync* self);
    InfraxError (*rwlock_write_lock)(InfraxSync* self);
    InfraxError (*rwlock_try_write_lock)(InfraxSync* self);
    InfraxError (*rwlock_write_unlock)(InfraxSync* self);

    InfraxError (*spinlock_lock)(InfraxSync* self);
    InfraxError (*spinlock_try_lock)(InfraxSync* self);
    InfraxError (*spinlock_unlock)(InfraxSync* self);

    InfraxError (*semaphore_wait)(InfraxSync* self);
    InfraxError (*semaphore_try_wait)(InfraxSync* self);
    InfraxError (*semaphore_post)(InfraxSync* self);
    InfraxError (*semaphore_get_value)(InfraxSync* self, int* value);

    InfraxError (*cond_wait)(InfraxSync* self, InfraxSync* mutex);
    InfraxError (*cond_timedwait)(InfraxSync* self, InfraxSync* mutex, InfraxTime timeout_ms);
    InfraxError (*cond_signal)(InfraxSync* self);
    InfraxError (*cond_broadcast)(InfraxSync* self);
    InfraxI64 (*cond_exchange)(InfraxSync* self, InfraxI64 value);
    InfraxBool (*cond_compare_exchange)(InfraxSync* self, InfraxI64* expected, InfraxI64 desired);
    InfraxError (*cond_fetch_add)(InfraxSync* self, InfraxI64 value);
    InfraxError (*cond_fetch_sub)(InfraxSync* self, InfraxI64 value);
    InfraxError (*cond_fetch_and)(InfraxSync* self, InfraxI64 value);
    InfraxError (*cond_fetch_or)(InfraxSync* self, InfraxI64 value);
    InfraxError (*cond_fetch_xor)(InfraxSync* self, InfraxI64 value);
    
    InfraxI64 (*atomic_load)(InfraxSync* self);
    void (*atomic_store)(InfraxSync* self, InfraxI64 value);
    InfraxI64 (*atomic_exchange)(InfraxSync* self, InfraxI64 value);
    InfraxBool (*atomic_compare_exchange)(InfraxSync* self, InfraxI64* expected, InfraxI64 desired);
    InfraxI64 (*atomic_fetch_add)(InfraxSync* self, InfraxI64 value);
    InfraxI64 (*atomic_fetch_sub)(InfraxSync* self, InfraxI64 value);
    InfraxI64 (*atomic_fetch_and)(InfraxSync* self, InfraxI64 value);
    InfraxI64 (*atomic_fetch_or)(InfraxSync* self, InfraxI64 value);
    InfraxI64 (*atomic_fetch_xor)(InfraxSync* self, InfraxI64 value);
};

// Opaque handle for synchronization primitives
// Size is set to accommodate the largest sync primitive across platforms
// Alignment is set to the strictest requirement
typedef struct {
    // union {
    //     pthread_mutex_t mutex;
    //     pthread_rwlock_t rwlock;
    //     pthread_spinlock_t spin;
    //     sem_t sem;
    //     pthread_cond_t cond;
    // } native_handle;    
    InfraxU64 data[8];  // 128 bytes, double the size to ensure enough space
} __attribute__((aligned(16))) InfraxSyncHandle;

// The instance structure
struct InfraxSync {
    InfraxSync* self;
    InfraxSyncClassType* klass;//InfraxSyncClass

    InfraxBool is_initialized;
    InfraxSyncType type;

    // Opaque handle for the native synchronization primitive
    InfraxSyncHandle handle;

    // For atomic operations
    //int64_t value;
    InfraxI64 value;
};

// The "static" interface instance (like Java's Class object)
extern InfraxSyncClassType InfraxSyncClass;

#endif // INFRAX_SYNC_H