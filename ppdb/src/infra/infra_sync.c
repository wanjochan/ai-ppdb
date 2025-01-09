/*
 * infra_sync.c - Synchronization Primitives Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

typedef struct {
    ppdb_thread_func_t func;
    void* arg;
} thread_wrapper_t;

static void* thread_wrapper(void* arg) {
    thread_wrapper_t* wrapper = (thread_wrapper_t*)arg;
    ppdb_thread_func_t func = wrapper->func;
    void* func_arg = wrapper->arg;
    free(wrapper);
    func(func_arg);
    return NULL;
}

ppdb_error_t ppdb_thread_create(ppdb_thread_t** thread, 
                               ppdb_thread_func_t func, void* arg) {
    if (!thread || !func) {
        return PPDB_ERR_PARAM;
    }

    ppdb_thread_t* new_thread = (ppdb_thread_t*)malloc(sizeof(ppdb_thread_t));
    if (!new_thread) {
        return PPDB_ERR_MEMORY;
    }

    thread_wrapper_t* wrapper = (thread_wrapper_t*)malloc(sizeof(thread_wrapper_t));
    if (!wrapper) {
        free(new_thread);
        return PPDB_ERR_MEMORY;
    }

    wrapper->func = func;
    wrapper->arg = arg;

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

    int err = pthread_create(&new_thread->thread, NULL, thread_wrapper, wrapper);
    if (err != 0) {
        free(wrapper);
        free(new_thread);
        return PPDB_ERR_THREAD;
    }

    new_thread->state = PPDB_THREAD_RUNNING;
    new_thread->start_time = time(NULL);

    *thread = new_thread;
    return PPDB_OK;
}

ppdb_error_t ppdb_thread_join(ppdb_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_join(thread->thread, NULL);
    if (err != 0) {
        return PPDB_ERR_THREAD;
    }

    thread->state = PPDB_THREAD_STOPPED;
    thread->stop_time = time(NULL);

    return PPDB_OK;
}

ppdb_error_t ppdb_thread_detach(ppdb_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_detach(thread->thread);
    if (err != 0) {
        return PPDB_ERR_THREAD;
    }

    return PPDB_OK;
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

ppdb_error_t ppdb_thread_set_name(ppdb_thread_t* thread, const char* name) {
    if (!thread || !name) {
        return PPDB_ERR_PARAM;
    }

    strncpy(thread->name, name, sizeof(thread->name) - 1);
    thread->name[sizeof(thread->name) - 1] = '\0';

    return PPDB_OK;
}

ppdb_error_t ppdb_thread_get_stats(ppdb_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    // Get thread CPU time and other stats
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
        thread->cpu_time = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    }

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

    int err = pthread_mutex_init(&new_mutex->mutex, NULL);
    if (err != 0) {
        free(new_mutex);
        return PPDB_ERR_MUTEX;
    }

    *mutex = new_mutex;
    return PPDB_OK;
}

ppdb_error_t ppdb_mutex_destroy(ppdb_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_mutex_destroy(&mutex->mutex);
    if (err != 0) {
        return PPDB_ERR_MUTEX;
    }

    free(mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_mutex_lock(ppdb_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_mutex_lock(&mutex->mutex);
    if (err != 0) {
        return PPDB_ERR_MUTEX;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_mutex_unlock(ppdb_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_mutex_unlock(&mutex->mutex);
    if (err != 0) {
        return PPDB_ERR_MUTEX;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_mutex_trylock(ppdb_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_mutex_trylock(&mutex->mutex);
    if (err == EBUSY) {
        return PPDB_ERR_BUSY;
    } else if (err != 0) {
        return PPDB_ERR_MUTEX;
    }

    return PPDB_OK;
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

    int err = pthread_cond_init(&new_cond->cond, NULL);
    if (err != 0) {
        free(new_cond);
        return PPDB_ERR_COND;
    }

    *cond = new_cond;
    return PPDB_OK;
}

ppdb_error_t ppdb_cond_destroy(ppdb_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_destroy(&cond->cond);
    if (err != 0) {
        return PPDB_ERR_COND;
    }

    free(cond);
    return PPDB_OK;
}

ppdb_error_t ppdb_cond_wait(ppdb_cond_t* cond, ppdb_mutex_t* mutex) {
    if (!cond || !mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_wait(&cond->cond, &mutex->mutex);
    if (err != 0) {
        return PPDB_ERR_COND;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_cond_signal(ppdb_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_signal(&cond->cond);
    if (err != 0) {
        return PPDB_ERR_COND;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_cond_broadcast(ppdb_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_broadcast(&cond->cond);
    if (err != 0) {
        return PPDB_ERR_COND;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_cond_timedwait(ppdb_cond_t* cond, ppdb_mutex_t* mutex, const struct timespec* abstime) {
    if (!cond || !mutex || !abstime) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_timedwait(&cond->cond, &mutex->mutex, abstime);
    if (err == ETIMEDOUT) {
        return PPDB_ERR_TIMEOUT;
    } else if (err != 0) {
        return PPDB_ERR_COND;
    }

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Read-Write Locks
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_rwlock_create(ppdb_rwlock_t** rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    ppdb_rwlock_t* new_rwlock = (ppdb_rwlock_t*)malloc(sizeof(ppdb_rwlock_t));
    if (!new_rwlock) {
        return PPDB_ERR_MEMORY;
    }

    int err = pthread_rwlock_init(&new_rwlock->rwlock, NULL);
    if (err != 0) {
        free(new_rwlock);
        return PPDB_ERR_RWLOCK;
    }

    *rwlock = new_rwlock;
    return PPDB_OK;
}

ppdb_error_t ppdb_rwlock_destroy(ppdb_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_rwlock_destroy(&rwlock->rwlock);
    if (err != 0) {
        return PPDB_ERR_RWLOCK;
    }

    free(rwlock);
    return PPDB_OK;
}

ppdb_error_t ppdb_rwlock_rdlock(ppdb_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_rwlock_rdlock(&rwlock->rwlock);
    if (err != 0) {
        return PPDB_ERR_RWLOCK;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_rwlock_wrlock(ppdb_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_rwlock_wrlock(&rwlock->rwlock);
    if (err != 0) {
        return PPDB_ERR_RWLOCK;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_rwlock_unlock(ppdb_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_rwlock_unlock(&rwlock->rwlock);
    if (err != 0) {
        return PPDB_ERR_RWLOCK;
    }

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_yield(void) {
    sched_yield();
    return PPDB_OK;
}

ppdb_error_t ppdb_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
    return PPDB_OK;
}

