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
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_error.h"

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

infra_error_t infra_platform_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg) {
    if (!thread || !func) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    pthread_t* handle = malloc(sizeof(pthread_t));
    if (!handle) {
        return INFRA_ERROR_NO_MEMORY;
    }

    if (pthread_create(handle, NULL, func, arg) != 0) {
        free(handle);
        return INFRA_ERROR_SYSTEM;
    }

    *thread = handle;
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

infra_error_t infra_platform_mutex_create(infra_mutex_t* mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    pthread_mutex_t* handle = malloc(sizeof(pthread_mutex_t));
    if (!handle) {
        return INFRA_ERROR_NO_MEMORY;
    }

    if (pthread_mutex_init(handle, NULL) != 0) {
        free(handle);
        return INFRA_ERROR_SYSTEM;
    }

    *mutex = handle;
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

infra_error_t infra_platform_cond_create(infra_cond_t* cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    pthread_cond_t* handle = malloc(sizeof(pthread_cond_t));
    if (!handle) {
        return INFRA_ERROR_NO_MEMORY;
    }

    if (pthread_cond_init(handle, NULL) != 0) {
        free(handle);
        return INFRA_ERROR_SYSTEM;
    }

    *cond = handle;
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

infra_error_t infra_platform_rwlock_create(infra_rwlock_t* rwlock) {
    if (!rwlock) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    pthread_rwlock_t* handle = malloc(sizeof(pthread_rwlock_t));
    if (!handle) {
        return INFRA_ERROR_NO_MEMORY;
    }

    if (pthread_rwlock_init(handle, NULL) != 0) {
        free(handle);
        return INFRA_ERROR_SYSTEM;
    }

    *rwlock = handle;
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

infra_error_t infra_file_open(const char* path, infra_flags_t flags, int mode, INFRA_CORE_Handle_t* handle) {
    if (!path || !handle) {
        return INFRA_ERROR_INVALID;
    }

    // 转换 flags 到系统标志
    int os_flags = 0;
    if (flags & INFRA_FILE_RDONLY) os_flags |= O_RDONLY;
    if (flags & INFRA_FILE_WRONLY) os_flags |= O_WRONLY;
    if (flags & INFRA_FILE_RDWR)   os_flags |= O_RDWR;
    if (flags & INFRA_FILE_CREATE) os_flags |= O_CREAT;
    if (flags & INFRA_FILE_APPEND) os_flags |= O_APPEND;
    if (flags & INFRA_FILE_TRUNC)  os_flags |= O_TRUNC;

    infra_printf("Opening file %s with flags 0x%x (os_flags 0x%x) mode %02o\n", 
                 path, flags, os_flags, mode);
    
    int fd = open(path, os_flags, mode);
    if (fd < 0) {
        if (!infra_is_expected_error(INFRA_ERROR_IO)) {
            infra_printf("[UNEXPECTED] Failed to open file: %s (errno: %d)\n", strerror(errno), errno);
        } else {
            infra_printf("[EXPECTED] Failed to open file: %s (errno: %d)\n", strerror(errno), errno);
        }
        return INFRA_ERROR_IO;
    }
    
    *handle = fd;
    return INFRA_OK;
} 