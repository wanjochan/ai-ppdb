/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_sync.c - Synchronization Primitives Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t* mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_create((void**)mutex);
}

void infra_mutex_destroy(infra_mutex_t mutex) {
    if (mutex) {
        infra_platform_mutex_destroy(mutex);
    }
}

infra_error_t infra_mutex_lock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_lock(mutex);
}

infra_error_t infra_mutex_trylock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_trylock(mutex);
}

infra_error_t infra_mutex_unlock(infra_mutex_t mutex) {
    if (!mutex) return INFRA_ERROR_INVALID;
    return infra_platform_mutex_unlock(mutex);
}

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_cond_create(infra_cond_t* cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_create((void**)cond);
}

void infra_cond_destroy(infra_cond_t cond) {
    if (cond) {
        infra_platform_cond_destroy(cond);
    }
}

infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex) {
    if (!cond || !mutex) return INFRA_ERROR_INVALID;
    return infra_platform_cond_wait(cond, mutex);
}

infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, uint64_t timeout_ms) {
    if (!cond || !mutex) return INFRA_ERROR_INVALID;
    return infra_platform_cond_timedwait(cond, mutex, timeout_ms);
}

infra_error_t infra_cond_signal(infra_cond_t cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_signal(cond);
}

infra_error_t infra_cond_broadcast(infra_cond_t cond) {
    if (!cond) return INFRA_ERROR_INVALID;
    return infra_platform_cond_broadcast(cond);
}

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_rwlock_create(infra_rwlock_t* rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_create((void**)rwlock);
}

void infra_rwlock_destroy(infra_rwlock_t rwlock) {
    if (rwlock) {
        infra_platform_rwlock_destroy(rwlock);
    }
}

infra_error_t infra_rwlock_rdlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_rdlock(rwlock);
}

infra_error_t infra_rwlock_tryrdlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_tryrdlock(rwlock);
}

infra_error_t infra_rwlock_wrlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_wrlock(rwlock);
}

infra_error_t infra_rwlock_trywrlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_trywrlock(rwlock);
}

infra_error_t infra_rwlock_unlock(infra_rwlock_t rwlock) {
    if (!rwlock) return INFRA_ERROR_INVALID;
    return infra_platform_rwlock_unlock(rwlock);
}

//-----------------------------------------------------------------------------
// Thread Operations
//-----------------------------------------------------------------------------

infra_error_t infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg) {
    if (!thread || !func) return INFRA_ERROR_INVALID;
    return infra_platform_thread_create((void**)thread, func, arg);
}

infra_error_t infra_thread_join(infra_thread_t thread) {
    if (!thread) return INFRA_ERROR_INVALID;
    return infra_platform_thread_join(thread);
}

infra_error_t infra_thread_detach(infra_thread_t thread) {
    if (!thread) return INFRA_ERROR_INVALID;
    return infra_platform_thread_detach(thread);
}

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

infra_error_t infra_yield(void) {
    return infra_platform_yield();
}

infra_error_t infra_sleep(uint32_t milliseconds) {
    return infra_platform_sleep(milliseconds);
}

