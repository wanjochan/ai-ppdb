/*
 * base_sync.inc.c - Synchronization Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Thread functions
static void* thread_wrapper(void* arg) {
    struct {
        ppdb_base_thread_func_t func;
        void* arg;
    }* wrapper = arg;
    
    ppdb_base_thread_func_t func = wrapper->func;
    void* func_arg = wrapper->arg;
    free(wrapper);
    
    func(func_arg);
    return NULL;
}

ppdb_error_t ppdb_base_thread_create(ppdb_base_thread_t** thread, ppdb_base_thread_func_t func, void* arg) {
    if (!thread || !func) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_thread_t* new_thread = (ppdb_base_thread_t*)malloc(sizeof(ppdb_base_thread_t));
    if (!new_thread) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_thread, 0, sizeof(ppdb_base_thread_t));

    // Create wrapper data
    struct {
        ppdb_base_thread_func_t func;
        void* arg;
    }* wrapper = malloc(sizeof(*wrapper));
    if (!wrapper) {
        free(new_thread);
        return PPDB_BASE_ERR_MEMORY;
    }
    wrapper->func = func;
    wrapper->arg = arg;

    if (pthread_create(&new_thread->thread, NULL, thread_wrapper, wrapper) != 0) {
        free(wrapper);
        free(new_thread);
        return PPDB_BASE_ERR_SYSTEM;
    }

    new_thread->initialized = true;
    *thread = new_thread;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_join(ppdb_base_thread_t* thread) {
    if (!thread || !thread->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_join(thread->thread, NULL) != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_thread_detach(ppdb_base_thread_t* thread) {
    if (!thread || !thread->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_detach(thread->thread) != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}

void ppdb_base_yield(void) {
    sched_yield();
}

void ppdb_base_sleep(uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// Mutex functions
ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex) {
    if (!mutex) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_mutex_t* new_mutex = (ppdb_base_mutex_t*)malloc(sizeof(ppdb_base_mutex_t));
    if (!new_mutex) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_mutex, 0, sizeof(ppdb_base_mutex_t));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    if (pthread_mutex_init(&new_mutex->mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(new_mutex);
        return PPDB_BASE_ERR_SYSTEM;
    }

    pthread_mutexattr_destroy(&attr);
    new_mutex->initialized = true;
    *mutex = new_mutex;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_mutex_destroy(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    mutex->initialized = false;
    free(mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex) {
    if (!mutex || !mutex->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    int ret = pthread_mutex_trylock(&mutex->mutex);
    if (ret == EBUSY) {
        return PPDB_BASE_ERR_BUSY;
    } else if (ret != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}