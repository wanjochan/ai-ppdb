#ifndef INFRAX_SYNC_H
#define INFRAX_SYNC_H

//sync primitive
//design pattern: factory (by type)

#include "libc/thread/thread.h"
#include "libc/thread/semaphore.h"
#include "libc/atomic.h"
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
};

// The instance structure
struct InfraxSync {
    bool is_initialized;
    InfraxSyncType type;

    // Native handles for different synchronization primitives
    union {
        pthread_mutex_t mutex;
        pthread_rwlock_t rwlock;
        pthread_spinlock_t spin;
        sem_t sem;
        pthread_cond_t cond;
    } native_handle;

    // Instance methods
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
    int64_t (*cond_exchange)(InfraxSync* self, int64_t value);
    bool (*cond_compare_exchange)(InfraxSync* self, int64_t* expected, int64_t desired);
    InfraxError (*cond_fetch_add)(InfraxSync* self, int64_t value);
    InfraxError (*cond_fetch_sub)(InfraxSync* self, int64_t value);
    InfraxError (*cond_fetch_and)(InfraxSync* self, int64_t value);
    InfraxError (*cond_fetch_or)(InfraxSync* self, int64_t value);
    InfraxError (*cond_fetch_xor)(InfraxSync* self, int64_t value);
    
    _Atomic int64_t value;
    int64_t (*atomic_load)(InfraxSync* self);
    void (*atomic_store)(InfraxSync* self, int64_t value);
    int64_t (*atomic_exchange)(InfraxSync* self, int64_t value);
    bool (*atomic_compare_exchange)(InfraxSync* self, int64_t* expected, int64_t desired);
    int64_t (*atomic_fetch_add)(InfraxSync* self, int64_t value);
    int64_t (*atomic_fetch_sub)(InfraxSync* self, int64_t value);
    int64_t (*atomic_fetch_and)(InfraxSync* self, int64_t value);
    int64_t (*atomic_fetch_or)(InfraxSync* self, int64_t value);
    int64_t (*atomic_fetch_xor)(InfraxSync* self, int64_t value);
};

// The "static" interface instance (like Java's Class object)
extern const InfraxSyncClassType InfraxSyncClass;

#endif // INFRAX_SYNC_H