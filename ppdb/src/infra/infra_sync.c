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

ppdb_error_t ppdb_thread_create(ppdb_thread_t** thread, 
                               ppdb_thread_func_t func, void* arg) {
    if (!thread || !func) {
        return PPDB_ERR_PARAM;
    }

    ppdb_thread_t* new_thread = (ppdb_thread_t*)malloc(sizeof(ppdb_thread_t));
    if (!new_thread) {
        return PPDB_ERR_MEMORY;
    }

    // Initialize thread state
    new_thread->state = PPDB_THREAD_INIT;
    new_thread->func = func;
    new_thread->arg = arg;
    new_thread->flags = 0;
    new_thread->start_time = 0;
    new_thread->stop_time = 0;
    new_thread->cpu_time = 0;
    memset(&new_thread->stats, 0, sizeof(new_thread->stats));
    memset(new_thread->name, 0, sizeof(new_thread->name));

    infra_error_t err = infra_thread_create(&new_thread->thread, func, arg);
    if (err != INFRA_OK) {
        free(new_thread);
        return err;
    }

    new_thread->state = PPDB_THREAD_RUNNING;
    new_thread->start_time = infra_time_monotonic_ms();

    *thread = new_thread;
    return PPDB_OK;
}

ppdb_error_t ppdb_thread_join(ppdb_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    infra_error_t err = infra_thread_join(thread->thread, NULL);
    if (err != INFRA_OK) {
        return err;
    }

    thread->state = PPDB_THREAD_STOPPED;
    thread->stop_time = infra_time_monotonic_ms();

    return PPDB_OK;
}

ppdb_error_t ppdb_thread_detach(ppdb_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    return infra_thread_detach(thread->thread);
}

ppdb_error_t ppdb_thread_destroy(ppdb_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    if (thread->state == PPDB_THREAD_RUNNING) {
        return PPDB_ERR_THREAD;
    }

    free(thread);
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_mutex_create(ppdb_mutex_t** mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    ppdb_mutex_t* new_mutex = (ppdb_mutex_t*)malloc(sizeof(ppdb_mutex_t));
    if (!new_mutex) {
        return PPDB_ERR_MEMORY;
    }

    infra_error_t err = infra_mutex_create(&new_mutex->mutex);
    if (err != INFRA_OK) {
        free(new_mutex);
        return err;
    }

    *mutex = new_mutex;
    return PPDB_OK;
}

ppdb_error_t ppdb_mutex_destroy(ppdb_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    infra_error_t err = infra_mutex_destroy(mutex->mutex);
    if (err != INFRA_OK) {
        return err;
    }

    free(mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_mutex_lock(ppdb_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    return infra_mutex_lock(mutex->mutex);
}

ppdb_error_t ppdb_mutex_unlock(ppdb_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    return infra_mutex_unlock(mutex->mutex);
}

ppdb_error_t ppdb_mutex_trylock(ppdb_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    return infra_mutex_trylock(mutex->mutex);
}

//-----------------------------------------------------------------------------
// Condition Variables
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_cond_create(ppdb_cond_t** cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    ppdb_cond_t* new_cond = (ppdb_cond_t*)malloc(sizeof(ppdb_cond_t));
    if (!new_cond) {
        return PPDB_ERR_MEMORY;
    }

    infra_error_t err = infra_cond_create(&new_cond->cond);
    if (err != INFRA_OK) {
        free(new_cond);
        return err;
    }

    *cond = new_cond;
    return PPDB_OK;
}

ppdb_error_t ppdb_cond_destroy(ppdb_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    infra_error_t err = infra_cond_destroy(cond->cond);
    if (err != INFRA_OK) {
        return err;
    }

    free(cond);
    return PPDB_OK;
}

ppdb_error_t ppdb_cond_wait(ppdb_cond_t* cond, ppdb_mutex_t* mutex) {
    if (!cond || !mutex) {
        return PPDB_ERR_PARAM;
    }

    return infra_cond_wait(cond->cond, mutex->mutex);
}

ppdb_error_t ppdb_cond_signal(ppdb_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    return infra_cond_signal(cond->cond);
}

ppdb_error_t ppdb_cond_broadcast(ppdb_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    return infra_cond_broadcast(cond->cond);
}

ppdb_error_t ppdb_cond_timedwait(ppdb_cond_t* cond, ppdb_mutex_t* mutex,
                                const struct timespec* abstime) {
    if (!cond || !mutex || !abstime) {
        return PPDB_ERR_PARAM;
    }

    return infra_cond_timedwait(cond->cond, mutex->mutex, abstime);
}

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_rwlock_create(ppdb_rwlock_t** rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    ppdb_rwlock_t* new_rwlock = (ppdb_rwlock_t*)malloc(sizeof(ppdb_rwlock_t));
    if (!new_rwlock) {
        return PPDB_ERR_MEMORY;
    }

    infra_error_t err = infra_rwlock_create(&new_rwlock->rwlock);
    if (err != INFRA_OK) {
        free(new_rwlock);
        return err;
    }

    *rwlock = new_rwlock;
    return PPDB_OK;
}

ppdb_error_t ppdb_rwlock_destroy(ppdb_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    infra_error_t err = infra_rwlock_destroy(rwlock->rwlock);
    if (err != INFRA_OK) {
        return err;
    }

    free(rwlock);
    return PPDB_OK;
}

ppdb_error_t ppdb_rwlock_rdlock(ppdb_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    return infra_rwlock_rdlock(rwlock->rwlock);
}

ppdb_error_t ppdb_rwlock_wrlock(ppdb_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    return infra_rwlock_wrlock(rwlock->rwlock);
}

ppdb_error_t ppdb_rwlock_unlock(ppdb_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    return infra_rwlock_unlock(rwlock->rwlock);
}

//-----------------------------------------------------------------------------
// Misc Operations
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_yield(void) {
    struct timespec ts = {0, 0};
    return infra_time_sleep(&ts);
}

ppdb_error_t ppdb_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    return infra_time_sleep(&ts);
}

