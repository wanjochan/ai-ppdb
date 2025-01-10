/*
 * infra_sync.h - Synchronization Primitives Interface
 */

#ifndef INFRA_SYNC_H
#define INFRA_SYNC_H

#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"

//-----------------------------------------------------------------------------
// Types and Constants
//-----------------------------------------------------------------------------

typedef struct infra_thread infra_thread_t;
typedef struct infra_mutex infra_mutex_t;
typedef struct infra_cond infra_cond_t;
typedef struct infra_rwlock infra_rwlock_t;

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

typedef void (*infra_thread_func_t)(void* arg);

infra_error_t infra_thread_create(infra_thread_t** thread,
                                 infra_thread_func_t func,
                                 void* arg);
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

#endif /* INFRA_SYNC_H */ 