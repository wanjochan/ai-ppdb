/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_sync.c - Synchronization Primitives Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_platform.h"

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

infra_error_t infra_thread_create(infra_thread_t** thread, 
                                 infra_thread_func_t func, void* arg) {
    if (!thread || !func) {
        return INFRA_ERROR_INVALID;
    }

    infra_thread_t* new_thread = (infra_thread_t*)infra_malloc(sizeof(infra_thread_t));
    if (!new_thread) {
        return INFRA_ERROR_MEMORY;
    }

    // Initialize thread state
    new_thread->state = INFRA_THREAD_INIT;
    new_thread->func = func;
    new_thread->arg = arg;
    new_thread->flags = 0;
    new_thread->start_time = 0;
    new_thread->stop_time = 0;
    new_thread->cpu_time = 0;
    infra_memset(&new_thread->stats, 0, sizeof(new_thread->stats));
    infra_memset(new_thread->name, 0, sizeof(new_thread->name));

    infra_error_t err = infra_platform_thread_create(&new_thread->handle, func, arg);
    if (err != INFRA_OK) {
        infra_free(new_thread);
        return err;
    }

    new_thread->state = INFRA_THREAD_RUNNING;
    *thread = new_thread;
    return INFRA_OK;
}

infra_error_t infra_thread_join(infra_thread_t* thread) {
    if (!thread || !thread->handle) {
        return INFRA_ERROR_INVALID;
    }

    infra_error_t err = infra_platform_thread_join(thread->handle);
    if (err == INFRA_OK) {
        thread->state = INFRA_THREAD_STOPPED;
    }
    return err;
}

infra_error_t infra_thread_detach(infra_thread_t* thread) {
    if (!thread || !thread->handle) {
        return INFRA_ERROR_INVALID;
    }

    infra_error_t err = infra_platform_thread_detach(thread->handle);
    if (err == INFRA_OK) {
        thread->state = INFRA_THREAD_DETACHED;
    }
    return err;
}

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t** mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    infra_mutex_t* new_mutex = (infra_mutex_t*)infra_malloc(sizeof(infra_mutex_t));
    if (!new_mutex) {
        return INFRA_ERROR_MEMORY;
    }

    infra_error_t err = infra_platform_mutex_create(&new_mutex->handle);
    if (err != INFRA_OK) {
        infra_free(new_mutex);
        return err;
    }

    *mutex = new_mutex;
    return INFRA_OK;
}

void infra_mutex_destroy(infra_mutex_t* mutex) {
    if (!mutex || !mutex->handle) {
        return;
    }

    infra_platform_mutex_destroy(mutex->handle);
    infra_free(mutex);
}

infra_error_t infra_mutex_lock(infra_mutex_t* mutex) {
    if (!mutex || !mutex->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_mutex_lock(mutex->handle);
}

infra_error_t infra_mutex_unlock(infra_mutex_t* mutex) {
    if (!mutex || !mutex->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_mutex_unlock(mutex->handle);
}

infra_error_t infra_mutex_trylock(infra_mutex_t* mutex) {
    if (!mutex || !mutex->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_mutex_trylock(mutex->handle);
}

//-----------------------------------------------------------------------------
// Condition Variables
//-----------------------------------------------------------------------------

infra_error_t infra_cond_create(infra_cond_t** cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID;
    }

    infra_cond_t* new_cond = (infra_cond_t*)infra_malloc(sizeof(infra_cond_t));
    if (!new_cond) {
        return INFRA_ERROR_MEMORY;
    }

    infra_error_t err = infra_platform_cond_create(&new_cond->handle);
    if (err != INFRA_OK) {
        infra_free(new_cond);
        return err;
    }

    *cond = new_cond;
    return INFRA_OK;
}

void infra_cond_destroy(infra_cond_t* cond) {
    if (!cond || !cond->handle) {
        return;
    }

    infra_platform_cond_destroy(cond->handle);
    infra_free(cond);
}

infra_error_t infra_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex) {
    if (!cond || !cond->handle || !mutex || !mutex->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_cond_wait(cond->handle, mutex->handle);
}

infra_error_t infra_cond_timedwait(infra_cond_t* cond, infra_mutex_t* mutex, uint64_t timeout_ms) {
    if (!cond || !cond->handle || !mutex || !mutex->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_cond_timedwait(cond->handle, mutex->handle, timeout_ms);
}

infra_error_t infra_cond_signal(infra_cond_t* cond) {
    if (!cond || !cond->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_cond_signal(cond->handle);
}

infra_error_t infra_cond_broadcast(infra_cond_t* cond) {
    if (!cond || !cond->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_cond_broadcast(cond->handle);
}

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_rwlock_create(infra_rwlock_t** rwlock) {
    if (!rwlock) {
        return INFRA_ERROR_INVALID;
    }

    infra_rwlock_t* new_rwlock = (infra_rwlock_t*)infra_malloc(sizeof(infra_rwlock_t));
    if (!new_rwlock) {
        return INFRA_ERROR_MEMORY;
    }

    infra_error_t err = infra_platform_rwlock_create(&new_rwlock->handle);
    if (err != INFRA_OK) {
        infra_free(new_rwlock);
        return err;
    }

    *rwlock = new_rwlock;
    return INFRA_OK;
}

void infra_rwlock_destroy(infra_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->handle) {
        return;
    }

    infra_platform_rwlock_destroy(rwlock->handle);
    infra_free(rwlock);
}

infra_error_t infra_rwlock_rdlock(infra_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_rwlock_rdlock(rwlock->handle);
}

infra_error_t infra_rwlock_tryrdlock(infra_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_rwlock_tryrdlock(rwlock->handle);
}

infra_error_t infra_rwlock_wrlock(infra_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_rwlock_wrlock(rwlock->handle);
}

infra_error_t infra_rwlock_trywrlock(infra_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_rwlock_trywrlock(rwlock->handle);
}

infra_error_t infra_rwlock_unlock(infra_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->handle) {
        return INFRA_ERROR_INVALID;
    }

    return infra_platform_rwlock_unlock(rwlock->handle);
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

