/*
 * base_sync.inc.c - Synchronization Primitives Implementation
 *
 * This file contains:
 * 1. Thread management
 * 2. Mutex operations
 * 3. Condition variables
 * 4. Read-write locks
 */

#include <cosmopolitan.h>
#include "internal/base.h"

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

typedef struct {
    ppdb_base_thread_func_t func;
    void* arg;
} thread_wrapper_t;

static void* thread_wrapper(void* arg) {
    thread_wrapper_t* wrapper = (thread_wrapper_t*)arg;
    ppdb_base_thread_func_t func = wrapper->func;
    void* func_arg = wrapper->arg;
    free(wrapper);
    func(func_arg);
    return NULL;
}

ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, 
                                   ppdb_base_thread_func_t func, void* arg) {
    if (!thread || !func) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_thread_t* new_thread = (ppdb_base_thread_t*)malloc(sizeof(ppdb_base_thread_t));
    if (!new_thread) {
        return PPDB_BASE_ERR_MEMORY;
    }

    thread_wrapper_t* wrapper = (thread_wrapper_t*)malloc(sizeof(thread_wrapper_t));
    if (!wrapper) {
        free(new_thread);
        return PPDB_BASE_ERR_MEMORY;
    }

    wrapper->func = func;
    wrapper->arg = arg;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (pthread_create(&new_thread->thread, &attr, thread_wrapper, wrapper) != 0) {
        pthread_attr_destroy(&attr);
        free(wrapper);
        free(new_thread);
        return PPDB_BASE_ERR_THREAD;
    }

    pthread_attr_destroy(&attr);
    *thread = new_thread;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread) {
    if (!thread) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_join(thread->thread, NULL) != 0) {
        return PPDB_BASE_ERR_THREAD;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread) {
    if (!thread) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_detach(thread->thread) != 0) {
        return PPDB_BASE_ERR_THREAD;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_destroy(ppdb_base_thread_t* thread) {
    if (!thread) {
        return PPDB_BASE_ERR_PARAM;
    }
    free(thread);
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_mutex_t* new_mutex = (ppdb_base_mutex_t*)malloc(sizeof(ppdb_base_mutex_t));
    if (!new_mutex) {
        return PPDB_BASE_ERR_MEMORY;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    if (pthread_mutex_init(&new_mutex->mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(new_mutex);
        return PPDB_BASE_ERR_MUTEX;
    }

    pthread_mutexattr_destroy(&attr);
    *mutex = new_mutex;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_mutex_destroy(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }
    free(mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }
    int ret = pthread_mutex_trylock(&mutex->mutex);
    if (ret == EBUSY) {
        return PPDB_BASE_ERR_BUSY;
    } else if (ret != 0) {
        return PPDB_BASE_ERR_MUTEX;
    }
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Condition Variables
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_cond_create(ppdb_base_cond_t** cond) {
    if (!cond) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_cond_t* new_cond = (ppdb_base_cond_t*)malloc(sizeof(ppdb_base_cond_t));
    if (!new_cond) {
        return PPDB_BASE_ERR_MEMORY;
    }

    if (pthread_cond_init(&new_cond->cond, NULL) != 0) {
        free(new_cond);
        return PPDB_BASE_ERR_COND;
    }

    *cond = new_cond;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_destroy(ppdb_base_cond_t* cond) {
    if (!cond) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_cond_destroy(&cond->cond) != 0) {
        return PPDB_BASE_ERR_COND;
    }
    free(cond);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_wait(ppdb_base_cond_t* cond, ppdb_base_mutex_t* mutex) {
    if (!cond || !mutex) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_cond_wait(&cond->cond, &mutex->mutex) != 0) {
        return PPDB_BASE_ERR_COND;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_timedwait(ppdb_base_cond_t* cond, 
                                     ppdb_base_mutex_t* mutex,
                                     uint64_t timeout_us) {
    if (!cond || !mutex) {
        return PPDB_BASE_ERR_PARAM;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_us / 1000000;
    ts.tv_nsec += (timeout_us % 1000000) * 1000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    int ret = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
    if (ret == ETIMEDOUT) {
        return PPDB_BASE_ERR_TIMEOUT;
    } else if (ret != 0) {
        return PPDB_BASE_ERR_COND;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_signal(ppdb_base_cond_t* cond) {
    if (!cond) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_cond_signal(&cond->cond) != 0) {
        return PPDB_BASE_ERR_COND;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_broadcast(ppdb_base_cond_t* cond) {
    if (!cond) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_cond_broadcast(&cond->cond) != 0) {
        return PPDB_BASE_ERR_COND;
    }
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Read-Write Locks
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_rwlock_create(ppdb_base_rwlock_t** rwlock) {
    if (!rwlock) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_rwlock_t* new_rwlock = (ppdb_base_rwlock_t*)malloc(sizeof(ppdb_base_rwlock_t));
    if (!new_rwlock) {
        return PPDB_BASE_ERR_MEMORY;
    }

    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    #ifdef PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
    #endif

    if (pthread_rwlock_init(&new_rwlock->rwlock, &attr) != 0) {
        pthread_rwlockattr_destroy(&attr);
        free(new_rwlock);
        return PPDB_BASE_ERR_RWLOCK;
    }

    pthread_rwlockattr_destroy(&attr);
    *rwlock = new_rwlock;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_destroy(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_rwlock_destroy(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_RWLOCK;
    }
    free(rwlock);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_rdlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_rwlock_rdlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_RWLOCK;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_wrlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_rwlock_wrlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_RWLOCK;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_unlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_BASE_ERR_PARAM;
    }
    if (pthread_rwlock_unlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_RWLOCK;
    }
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Thread Control
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_yield(void) {
    sched_yield();
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
    return PPDB_OK;
} 