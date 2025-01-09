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
    ppdb_base_mem_free(wrapper);
    func(func_arg);
    return NULL;
}

ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, 
                                   ppdb_base_thread_func_t func, void* arg) {
    if (!thread || !func) {
        return PPDB_ERR_PARAM;
    }

    void* thread_ptr;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_thread_t), &thread_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_base_thread_t* new_thread = (ppdb_base_thread_t*)thread_ptr;

    void* wrapper_ptr;
    err = ppdb_base_mem_malloc(sizeof(thread_wrapper_t), &wrapper_ptr);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_thread);
        return err;
    }
    thread_wrapper_t* wrapper = (thread_wrapper_t*)wrapper_ptr;

    wrapper->func = func;
    wrapper->arg = arg;

    err = pthread_create(&new_thread->thread, NULL, thread_wrapper, wrapper);
    if (err != 0) {
        ppdb_base_mem_free(wrapper);
        ppdb_base_mem_free(new_thread);
        return PPDB_ERR_THREAD;
    }

    *thread = new_thread;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_join(thread->thread, NULL);
    if (err != 0) {
        return PPDB_ERR_THREAD;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_detach(thread->thread);
    if (err != 0) {
        return PPDB_ERR_THREAD;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_destroy(ppdb_base_thread_t* thread) {
    if (!thread) {
        return PPDB_ERR_PARAM;
    }

    ppdb_base_mem_free(thread);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_set_affinity(ppdb_base_thread_t* thread, int cpu_id) {
    if (!thread || cpu_id < 0) {
        return PPDB_ERR_PARAM;
    }

#ifdef _WIN32
    DWORD_PTR mask = 1ULL << cpu_id;
    if (!SetThreadAffinityMask(thread->thread, mask)) {
        return PPDB_ERR_THREAD;
    }
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    if (pthread_setaffinity_np(thread->thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return PPDB_ERR_THREAD;
    }
#endif

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    void* mutex_ptr;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_mutex_t), &mutex_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_base_mutex_t* new_mutex = (ppdb_base_mutex_t*)mutex_ptr;

    err = pthread_mutex_init(&new_mutex->mutex, NULL);
    if (err != 0) {
        ppdb_base_mem_free(new_mutex);
        return PPDB_ERR_MUTEX;
    }

    *mutex = new_mutex;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_mutex_destroy(&mutex->mutex);
    if (err != 0) {
        return PPDB_ERR_MUTEX;
    }

    ppdb_base_mem_free(mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_mutex_lock(&mutex->mutex);
    if (err != 0) {
        return PPDB_ERR_MUTEX;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex) {
    if (!mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_mutex_unlock(&mutex->mutex);
    if (err != 0) {
        return PPDB_ERR_MUTEX;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex) {
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

ppdb_error_t ppdb_base_cond_create(ppdb_base_cond_t** cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    void* cond_ptr;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_cond_t), &cond_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_base_cond_t* new_cond = (ppdb_base_cond_t*)cond_ptr;

    err = pthread_cond_init(&new_cond->cond, NULL);
    if (err != 0) {
        ppdb_base_mem_free(new_cond);
        return PPDB_ERR_COND;
    }

    *cond = new_cond;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_destroy(ppdb_base_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_destroy(&cond->cond);
    if (err != 0) {
        return PPDB_ERR_COND;
    }

    ppdb_base_mem_free(cond);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_wait(ppdb_base_cond_t* cond, ppdb_base_mutex_t* mutex) {
    if (!cond || !mutex) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_wait(&cond->cond, &mutex->mutex);
    if (err != 0) {
        return PPDB_ERR_COND;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_signal(ppdb_base_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_signal(&cond->cond);
    if (err != 0) {
        return PPDB_ERR_COND;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_cond_broadcast(ppdb_base_cond_t* cond) {
    if (!cond) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_cond_broadcast(&cond->cond);
    if (err != 0) {
        return PPDB_ERR_COND;
    }

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Read-Write Locks
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_rwlock_create(ppdb_base_rwlock_t** rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    void* rwlock_ptr;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_rwlock_t), &rwlock_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_base_rwlock_t* new_rwlock = (ppdb_base_rwlock_t*)rwlock_ptr;

    err = pthread_rwlock_init(&new_rwlock->rwlock, NULL);
    if (err != 0) {
        ppdb_base_mem_free(new_rwlock);
        return PPDB_ERR_RWLOCK;
    }

    *rwlock = new_rwlock;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_destroy(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_rwlock_destroy(&rwlock->rwlock);
    if (err != 0) {
        return PPDB_ERR_RWLOCK;
    }

    ppdb_base_mem_free(rwlock);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_rdlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_rwlock_rdlock(&rwlock->rwlock);
    if (err != 0) {
        return PPDB_ERR_RWLOCK;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_wrlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock) {
        return PPDB_ERR_PARAM;
    }

    int err = pthread_rwlock_wrlock(&rwlock->rwlock);
    if (err != 0) {
        return PPDB_ERR_RWLOCK;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_unlock(ppdb_base_rwlock_t* rwlock) {
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