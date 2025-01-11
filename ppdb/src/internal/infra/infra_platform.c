/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_platform.c - Platform Abstraction Layer Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Platform-specific Functions
//-----------------------------------------------------------------------------

infra_error_t infra_platform_init(void) {
    return INFRA_OK;
}

void infra_platform_cleanup(void) {
}

infra_error_t infra_platform_get_pid(infra_pid_t* pid) {
    if (!pid) {
        return INFRA_ERROR_INVALID;
    }
    *pid = getpid();
    return INFRA_OK;
}

infra_error_t infra_platform_get_tid(infra_tid_t* tid) {
    if (!tid) {
        return INFRA_ERROR_INVALID;
    }
    *tid = pthread_self();
    return INFRA_OK;
}

infra_error_t infra_platform_sleep(uint32_t ms) {
    usleep(ms * 1000);
    return INFRA_OK;
}

infra_error_t infra_platform_yield(void) {
    sched_yield();
    return INFRA_OK;
}

infra_error_t infra_platform_get_time(infra_time_t* time) {
    if (!time) {
        return INFRA_ERROR_INVALID;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    *time = (infra_time_t)ts.tv_sec * 1000000000ULL + (infra_time_t)ts.tv_nsec;
    return INFRA_OK;
}

infra_error_t infra_platform_get_monotonic_time(infra_time_t* time) {
    if (!time) {
        return INFRA_ERROR_INVALID;
    }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *time = (infra_time_t)ts.tv_sec * 1000000000ULL + (infra_time_t)ts.tv_nsec;
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

infra_error_t infra_platform_thread_create(void** handle, infra_thread_func_t func, void* arg) {
    if (!handle || !func) {
        return INFRA_ERROR_INVALID;
    }

    pthread_t* pthread = infra_malloc(sizeof(pthread_t));
    if (!pthread) {
        return INFRA_ERROR_MEMORY;
    }

    if (pthread_create(pthread, NULL, (void* (*)(void*))func, arg) != 0) {
        infra_free(pthread);
        return INFRA_ERROR_IO;
    }

    *handle = pthread;
    return INFRA_OK;
}

infra_error_t infra_platform_thread_join(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_t* pthread = (pthread_t*)handle;
    int ret = pthread_join(*pthread, NULL);
    infra_free(pthread);
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_platform_thread_detach(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_t* pthread = (pthread_t*)handle;
    int ret = pthread_detach(*pthread);
    infra_free(pthread);
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_mutex_create(void** handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* mutex = infra_malloc(sizeof(pthread_mutex_t));
    if (!mutex) {
        return INFRA_ERROR_MEMORY;
    }

    if (pthread_mutex_init(mutex, NULL) != 0) {
        infra_free(mutex);
        return INFRA_ERROR_IO;
    }

    *handle = mutex;
    return INFRA_OK;
}

void infra_platform_mutex_destroy(void* handle) {
    if (handle) {
        pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
        pthread_mutex_destroy(mutex);
        infra_free(mutex);
    }
}

infra_error_t infra_platform_mutex_lock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    return (pthread_mutex_lock(mutex) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_platform_mutex_trylock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    int ret = pthread_mutex_trylock(mutex);
    return (ret == 0) ? INFRA_OK : (ret == EBUSY ? INFRA_ERROR_BUSY : INFRA_ERROR_IO);
}

infra_error_t infra_platform_mutex_unlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    return (pthread_mutex_unlock(mutex) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_cond_create(void** handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond = infra_malloc(sizeof(pthread_cond_t));
    if (!cond) {
        return INFRA_ERROR_MEMORY;
    }

    if (pthread_cond_init(cond, NULL) != 0) {
        infra_free(cond);
        return INFRA_ERROR_IO;
    }

    *handle = cond;
    return INFRA_OK;
}

void infra_platform_cond_destroy(void* handle) {
    if (handle) {
        pthread_cond_t* cond = (pthread_cond_t*)handle;
        pthread_cond_destroy(cond);
        infra_free(cond);
    }
}

infra_error_t infra_platform_cond_wait(void* cond_handle, void* mutex_handle) {
    if (!cond_handle || !mutex_handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cond_handle;
    pthread_mutex_t* mutex = (pthread_mutex_t*)mutex_handle;
    return (pthread_cond_wait(cond, mutex) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_platform_cond_timedwait(void* cond_handle, void* mutex_handle, uint64_t timeout_ms) {
    if (!cond_handle || !mutex_handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cond_handle;
    pthread_mutex_t* mutex = (pthread_mutex_t*)mutex_handle;
    
    // 计算超时时间
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    // 等待条件变量
    int ret = pthread_cond_timedwait(cond, mutex, &ts);
    if (ret == 0) {
        return INFRA_OK;
    } else if (ret == ETIMEDOUT) {
        return INFRA_ERROR_TIMEOUT;
    } else {
        return INFRA_ERROR_IO;
    }
}

infra_error_t infra_platform_cond_signal(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond = (pthread_cond_t*)handle;
    return (pthread_cond_signal(cond) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_platform_cond_broadcast(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond = (pthread_cond_t*)handle;
    return (pthread_cond_broadcast(cond) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_rwlock_create(void** handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = infra_malloc(sizeof(pthread_rwlock_t));
    if (!rwlock) {
        return INFRA_ERROR_MEMORY;
    }

    if (pthread_rwlock_init(rwlock, NULL) != 0) {
        infra_free(rwlock);
        return INFRA_ERROR_IO;
    }

    *handle = rwlock;
    return INFRA_OK;
}

void infra_platform_rwlock_destroy(void* handle) {
    if (handle) {
        pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
        pthread_rwlock_destroy(rwlock);
        infra_free(rwlock);
    }
}

infra_error_t infra_platform_rwlock_rdlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_rdlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_platform_rwlock_tryrdlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    int ret = pthread_rwlock_tryrdlock(rwlock);
    return (ret == 0) ? INFRA_OK : (ret == EBUSY ? INFRA_ERROR_BUSY : INFRA_ERROR_IO);
}

infra_error_t infra_platform_rwlock_wrlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_wrlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_platform_rwlock_trywrlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    int ret = pthread_rwlock_trywrlock(rwlock);
    return (ret == 0) ? INFRA_OK : (ret == EBUSY ? INFRA_ERROR_BUSY : INFRA_ERROR_IO);
}

infra_error_t infra_platform_rwlock_unlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_unlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_mutex_create(infra_mutex_t* mutex) {
    pthread_mutex_t* mtx = malloc(sizeof(pthread_mutex_t));
    if (!mtx) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    if (pthread_mutex_init(mtx, NULL) != 0) {
        free(mtx);
        return INFRA_ERROR_INVALID;
    }
    
    *mutex = mtx;
    return INFRA_OK;
}

void infra_mutex_destroy(infra_mutex_t mutex) {
    if (mutex) {
        pthread_mutex_destroy(mutex);
        free(mutex);
    }
}

infra_error_t infra_mutex_lock(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_lock(mutex) != 0) {
        return INFRA_ERROR_INVALID;
    }
    
    return INFRA_OK;
}

infra_error_t infra_mutex_unlock(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    if (pthread_mutex_unlock(mutex) != 0) {
        return INFRA_ERROR_INVALID;
    }
    
    return INFRA_OK;
}

infra_error_t infra_cond_init(infra_cond_t* cond) {
    pthread_cond_t* cv = malloc(sizeof(pthread_cond_t));
    if (!cv) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    if (pthread_cond_init(cv, NULL) != 0) {
        free(cv);
        return INFRA_ERROR_INVALID;
    }
    
    *cond = cv;
    return INFRA_OK;
}

void infra_cond_destroy(infra_cond_t cond) {
    if (cond) {
        pthread_cond_destroy(cond);
        free(cond);
    }
}

infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex) {
    if (!cond || !mutex) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    if (pthread_cond_wait(cond, mutex) != 0) {
        return INFRA_ERROR_INVALID;
    }
    
    return INFRA_OK;
}

infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, uint32_t timeout_ms) {
    if (!cond || !mutex) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    int ret = pthread_cond_timedwait(cond, mutex, &ts);
    if (ret == ETIMEDOUT) {
        return INFRA_ERROR_TIMEOUT;
    } else if (ret != 0) {
        return INFRA_ERROR_INVALID;
    }
    
    return INFRA_OK;
}

infra_error_t infra_cond_signal(infra_cond_t cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    if (pthread_cond_signal(cond) != 0) {
        return INFRA_ERROR_INVALID;
    }
    
    return INFRA_OK;
}

infra_error_t infra_cond_broadcast(infra_cond_t cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    if (pthread_cond_broadcast(cond) != 0) {
        return INFRA_ERROR_INVALID;
    }
    
    return INFRA_OK;
}

infra_error_t infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg) {
    pthread_t* tid = malloc(sizeof(pthread_t));
    if (!tid) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    if (pthread_create(tid, NULL, func, arg) != 0) {
        free(tid);
        return INFRA_ERROR_INVALID;
    }
    
    *thread = tid;
    return INFRA_OK;
}

infra_error_t infra_thread_join(infra_thread_t thread) {
    if (!thread) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    void* retval;
    if (pthread_join(*(pthread_t*)thread, &retval) != 0) {
        return INFRA_ERROR_INVALID;
    }
    
    free(thread);
    return INFRA_OK;
}

void infra_thread_exit(void* retval) {
    pthread_exit(retval);
}

infra_thread_t infra_thread_self(void) {
    pthread_t* tid = malloc(sizeof(pthread_t));
    if (tid) {
        *tid = pthread_self();
    }
    return tid;
} 