/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_sync.h - Synchronization Primitives Interface
 */

#ifndef INFRA_SYNC_H_
#define INFRA_SYNC_H_

#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"

//-----------------------------------------------------------------------------
// Types and Constants
//-----------------------------------------------------------------------------

typedef enum {
    INFRA_THREAD_INIT,
    INFRA_THREAD_RUNNING,
    INFRA_THREAD_STOPPED,
    INFRA_THREAD_DETACHED
} infra_thread_state_t;

typedef struct {
    uint64_t user_time;
    uint64_t system_time;
    size_t peak_memory;
} infra_thread_stats_t;

struct infra_thread {
    void* handle;                    // Platform-specific thread handle
    infra_thread_state_t state;      // Thread state
    infra_thread_func_t func;        // Thread function
    void* arg;                       // Thread argument
    uint32_t flags;                  // Thread flags
    uint64_t start_time;            // Thread start time
    uint64_t stop_time;             // Thread stop time
    uint64_t cpu_time;              // Thread CPU time
    infra_thread_stats_t stats;      // Thread statistics
    char name[32];                  // Thread name
};

struct infra_mutex {
    void* handle;                    // Platform-specific mutex handle
};

struct infra_cond {
    void* handle;                    // Platform-specific condition variable handle
};

struct infra_rwlock {
    void* handle;                    // Platform-specific read-write lock handle
};

typedef struct infra_thread infra_thread_t;
typedef struct infra_mutex infra_mutex_t;
typedef struct infra_cond infra_cond_t;
typedef struct infra_rwlock infra_rwlock_t;
typedef void* (*infra_thread_func_t)(void*);

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

infra_error_t infra_thread_create(infra_thread_t** handle, infra_thread_func_t func, void* arg);
infra_error_t infra_thread_join(infra_thread_t* thread);
infra_error_t infra_thread_detach(infra_thread_t* thread);
void infra_thread_exit(void* retval);

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t** mutex);
void infra_mutex_destroy(infra_mutex_t* mutex);
infra_error_t infra_mutex_lock(infra_mutex_t* mutex);
infra_error_t infra_mutex_trylock(infra_mutex_t* mutex);
infra_error_t infra_mutex_unlock(infra_mutex_t* mutex);

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_cond_create(infra_cond_t** cond);
void infra_cond_destroy(infra_cond_t* cond);
infra_error_t infra_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex);
infra_error_t infra_cond_timedwait(infra_cond_t* cond,
                                  infra_mutex_t* mutex,
                                  uint64_t timeout_ms);
infra_error_t infra_cond_signal(infra_cond_t* cond);
infra_error_t infra_cond_broadcast(infra_cond_t* cond);

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_rwlock_create(infra_rwlock_t** rwlock);
void infra_rwlock_destroy(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_rdlock(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_tryrdlock(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_wrlock(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_trywrlock(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_unlock(infra_rwlock_t* rwlock);

#endif /* INFRA_SYNC_H_ */ 