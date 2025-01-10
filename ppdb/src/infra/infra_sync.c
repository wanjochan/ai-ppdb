/*
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

    infra_error_t err = infra_thread_create(&new_thread->thread, func, arg);
    if (err != INFRA_OK) {
        infra_free(new_thread);
        return err;
    }

    new_thread->state = INFRA_THREAD_RUNNING;
    new_thread->start_time = infra_time_monotonic_ms();

    *thread = new_thread;
    return INFRA_OK;
}

infra_error_t infra_thread_join(infra_thread_t* thread) {
    if (!thread) {
        return INFRA_ERROR_INVALID;
    }

    infra_error_t err = infra_thread_join(thread->thread, NULL);
    if (err != INFRA_OK) {
        return err;
    }

    thread->state = INFRA_THREAD_STOPPED;
    thread->stop_time = infra_time_monotonic_ms();

    return INFRA_OK;
}

infra_error_t infra_thread_detach(infra_thread_t* thread) {
    if (!thread) {
        return INFRA_ERROR_INVALID;
    }

    return infra_thread_detach(thread->thread);
}

infra_error_t infra_thread_destroy(infra_thread_t* thread) {
    if (!thread) {
        return INFRA_ERROR_INVALID;
    }

    if (thread->state == INFRA_THREAD_RUNNING) {
        return INFRA_ERROR_SYSTEM;
    }

    infra_free(thread);
    return INFRA_OK;
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

    infra_error_t err = infra_mutex_create(&new_mutex->mutex);
    if (err != INFRA_OK) {
        infra_free(new_mutex);
        return err;
    }

    *mutex = new_mutex;
    return INFRA_OK;
}

infra_error_t infra_mutex_destroy(infra_mutex_t* mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    infra_error_t err = infra_mutex_destroy(mutex->mutex);
    if (err != INFRA_OK) {
        return err;
    }

    infra_free(mutex);
    return INFRA_OK;
}

infra_error_t infra_mutex_lock(infra_mutex_t* mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    return infra_mutex_lock(mutex->mutex);
}

infra_error_t infra_mutex_unlock(infra_mutex_t* mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    return infra_mutex_unlock(mutex->mutex);
}

infra_error_t infra_mutex_trylock(infra_mutex_t* mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    return infra_mutex_trylock(mutex->mutex);
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

    infra_error_t err = infra_cond_create(&new_cond->cond);
    if (err != INFRA_OK) {
        infra_free(new_cond);
        return err;
    }

    *cond = new_cond;
    return INFRA_OK;
}

infra_error_t infra_cond_destroy(infra_cond_t* cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID;
    }

    infra_error_t err = infra_cond_destroy(cond->cond);
    if (err != INFRA_OK) {
        return err;
    }

    infra_free(cond);
    return INFRA_OK;
}

infra_error_t infra_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex) {
    if (!cond || !mutex) {
        return INFRA_ERROR_INVALID;
    }

    return infra_cond_wait(cond->cond, mutex->mutex);
}

infra_error_t infra_cond_signal(infra_cond_t* cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID;
    }

    return infra_cond_signal(cond->cond);
}

infra_error_t infra_cond_broadcast(infra_cond_t* cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID;
    }

    return infra_cond_broadcast(cond->cond);
}

infra_error_t infra_cond_timedwait(infra_cond_t* cond, infra_mutex_t* mutex,
                                  const struct timespec* abstime) {
    if (!cond || !mutex || !abstime) {
        return INFRA_ERROR_INVALID;
    }

    return infra_cond_timedwait(cond->cond, mutex->mutex, abstime);
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

    infra_error_t err = infra_rwlock_create(&new_rwlock->rwlock);
    if (err != INFRA_OK) {
        infra_free(new_rwlock);
        return err;
    }

    *rwlock = new_rwlock;
    return INFRA_OK;
}

infra_error_t infra_rwlock_destroy(infra_rwlock_t* rwlock) {
    if (!rwlock) {
        return INFRA_ERROR_INVALID;
    }

    infra_error_t err = infra_rwlock_destroy(rwlock->rwlock);
    if (err != INFRA_OK) {
        return err;
    }

    infra_free(rwlock);
    return INFRA_OK;
}

infra_error_t infra_rwlock_rdlock(infra_rwlock_t* rwlock) {
    if (!rwlock) {
        return INFRA_ERROR_INVALID;
    }

    return infra_rwlock_rdlock(rwlock->rwlock);
}

infra_error_t infra_rwlock_wrlock(infra_rwlock_t* rwlock) {
    if (!rwlock) {
        return INFRA_ERROR_INVALID;
    }

    return infra_rwlock_wrlock(rwlock->rwlock);
}

infra_error_t infra_rwlock_unlock(infra_rwlock_t* rwlock) {
    if (!rwlock) {
        return INFRA_ERROR_INVALID;
    }

    return infra_rwlock_unlock(rwlock->rwlock);
}

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

infra_error_t infra_yield(void) {
    struct timespec ts = {0, 0};
    return infra_sleep(&ts);
}

infra_error_t infra_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    return infra_sleep(&ts);
}

