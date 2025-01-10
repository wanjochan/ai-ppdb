/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_platform.h - Platform Abstraction Layer
 */

#ifndef INFRA_PLATFORM_H
#define INFRA_PLATFORM_H

#include "internal/infra/infra.h"

//-----------------------------------------------------------------------------
// Platform Detection
//-----------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
    #define INFRA_PLATFORM_WINDOWS
    typedef unsigned long infra_pid_t;
    typedef unsigned long infra_tid_t;
#else
    #define INFRA_PLATFORM_UNIX
    typedef long infra_pid_t;
    typedef long infra_tid_t;
#endif

//-----------------------------------------------------------------------------
// Event Types
//-----------------------------------------------------------------------------

#define INFRA_EVENT_NONE   0x00
#define INFRA_EVENT_READ   0x01
#define INFRA_EVENT_WRITE  0x02
#define INFRA_EVENT_ERROR  0x04
#define INFRA_EVENT_TIMER  0x08
#define INFRA_EVENT_SIGNAL 0x10

//-----------------------------------------------------------------------------
// Platform-specific Functions
//-----------------------------------------------------------------------------

infra_error_t infra_platform_init(void);
void infra_platform_cleanup(void);

infra_error_t infra_platform_get_pid(infra_pid_t* pid);
infra_error_t infra_platform_get_tid(infra_tid_t* tid);

infra_error_t infra_platform_sleep(uint32_t ms);
infra_error_t infra_platform_yield(void);

infra_error_t infra_platform_get_time(infra_time_t* time);
infra_error_t infra_platform_get_monotonic_time(infra_time_t* time);

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

infra_error_t infra_platform_thread_create(void** handle, infra_thread_func_t func, void* arg);
infra_error_t infra_platform_thread_join(void* handle);
infra_error_t infra_platform_thread_detach(void* handle);

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_mutex_create(void** handle);
void infra_platform_mutex_destroy(void* handle);
infra_error_t infra_platform_mutex_lock(void* handle);
infra_error_t infra_platform_mutex_trylock(void* handle);
infra_error_t infra_platform_mutex_unlock(void* handle);

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_cond_create(void** handle);
void infra_platform_cond_destroy(void* handle);
infra_error_t infra_platform_cond_wait(void* cond_handle, void* mutex_handle);
infra_error_t infra_platform_cond_timedwait(void* cond_handle, void* mutex_handle, uint64_t timeout_ms);
infra_error_t infra_platform_cond_signal(void* handle);
infra_error_t infra_platform_cond_broadcast(void* handle);

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_rwlock_create(void** handle);
void infra_platform_rwlock_destroy(void* handle);
infra_error_t infra_platform_rwlock_rdlock(void* handle);
infra_error_t infra_platform_rwlock_tryrdlock(void* handle);
infra_error_t infra_platform_rwlock_wrlock(void* handle);
infra_error_t infra_platform_rwlock_trywrlock(void* handle);
infra_error_t infra_platform_rwlock_unlock(void* handle);

#endif /* INFRA_PLATFORM_H */ 
