/*
 * infra_platform.c - Platform Abstraction Layer Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"

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
    *tid = gettid();
    return INFRA_OK;
}

infra_error_t infra_platform_sleep(uint32_t ms) {
    usleep(ms * 1000);
    return INFRA_OK;
}

infra_error_t infra_platform_yield(void) {
    yield();
    return INFRA_OK;
}

infra_error_t infra_platform_get_time(infra_time_t* time) {
    if (!time) {
        return INFRA_ERROR_INVALID;
    }
    *time = now();
    return INFRA_OK;
}

infra_error_t infra_platform_get_monotonic_time(infra_time_t* time) {
    if (!time) {
        return INFRA_ERROR_INVALID;
    }
    *time = nowl();
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
        return INFRA_ERROR_SYSTEM;
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
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_thread_detach(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_t* pthread = (pthread_t*)handle;
    int ret = pthread_detach(*pthread);
    infra_free(pthread);
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
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
        return INFRA_ERROR_SYSTEM;
    }

    *handle = mutex;
    return INFRA_OK;
}

void infra_platform_mutex_destroy(void* handle) {
    if (!handle) {
        return;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    pthread_mutex_destroy(mutex);
    infra_free(mutex);
}

infra_error_t infra_platform_mutex_lock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    return (pthread_mutex_lock(mutex) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_mutex_trylock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    int ret = pthread_mutex_trylock(mutex);
    if (ret == EBUSY) {
        return INFRA_ERROR_BUSY;
    }
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_mutex_unlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    return (pthread_mutex_unlock(mutex) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
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
        return INFRA_ERROR_SYSTEM;
    }

    *handle = cond;
    return INFRA_OK;
}

void infra_platform_cond_destroy(void* handle) {
    if (!handle) {
        return;
    }

    pthread_cond_t* cond = (pthread_cond_t*)handle;
    pthread_cond_destroy(cond);
    infra_free(cond);
}

infra_error_t infra_platform_cond_wait(void* cond_handle, void* mutex_handle) {
    if (!cond_handle || !mutex_handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cond_handle;
    pthread_mutex_t* mutex = (pthread_mutex_t*)mutex_handle;
    return (pthread_cond_wait(cond, mutex) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_cond_timedwait(void* cond_handle, void* mutex_handle, uint64_t timeout_ms) {
    if (!cond_handle || !mutex_handle) {
        return INFRA_ERROR_INVALID;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    pthread_cond_t* cond = (pthread_cond_t*)cond_handle;
    pthread_mutex_t* mutex = (pthread_mutex_t*)mutex_handle;
    int ret = pthread_cond_timedwait(cond, mutex, &ts);
    
    if (ret == ETIMEDOUT) {
        return INFRA_ERROR_TIMEOUT;
    }
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_cond_signal(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond = (pthread_cond_t*)handle;
    return (pthread_cond_signal(cond) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_cond_broadcast(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond = (pthread_cond_t*)handle;
    return (pthread_cond_broadcast(cond) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
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
        return INFRA_ERROR_SYSTEM;
    }

    *handle = rwlock;
    return INFRA_OK;
}

void infra_platform_rwlock_destroy(void* handle) {
    if (!handle) {
        return;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    pthread_rwlock_destroy(rwlock);
    infra_free(rwlock);
}

infra_error_t infra_platform_rwlock_rdlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_rdlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_rwlock_tryrdlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    int ret = pthread_rwlock_tryrdlock(rwlock);
    if (ret == EBUSY) {
        return INFRA_ERROR_BUSY;
    }
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_rwlock_wrlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_wrlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_rwlock_trywrlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    int ret = pthread_rwlock_trywrlock(rwlock);
    if (ret == EBUSY) {
        return INFRA_ERROR_BUSY;
    }
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_rwlock_unlock(void* handle) {
    if (!handle) {
        return INFRA_ERROR_INVALID;
    }

    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_unlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
} 