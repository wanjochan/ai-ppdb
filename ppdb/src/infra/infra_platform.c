/*
 * infra_platform.c - Platform Abstraction Layer Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"

//-----------------------------------------------------------------------------
// Thread Management Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_thread_create(infra_thread_t* thread, void* (*func)(void*), void* arg) {
    if (!thread || !func) {
        return INFRA_ERR_PARAM;
    }

    pthread_t* handle = malloc(sizeof(pthread_t));
    if (!handle) {
        return INFRA_ERR_MEMORY;
    }

    int ret = pthread_create(handle, NULL, func, arg);
    if (ret != 0) {
        free(handle);
        return INFRA_ERR_THREAD;
    }

    *thread = handle;
    return INFRA_OK;
}

infra_error_t infra_thread_join(infra_thread_t thread, void** retval) {
    if (!thread) {
        return INFRA_ERR_PARAM;
    }

    pthread_t* handle = (pthread_t*)thread;
    int ret = pthread_join(*handle, retval);
    
    free(handle);
    return (ret == 0) ? INFRA_OK : INFRA_ERR_THREAD;
}

infra_error_t infra_thread_detach(infra_thread_t thread) {
    if (!thread) {
        return INFRA_ERR_PARAM;
    }

    pthread_t* handle = (pthread_t*)thread;
    int ret = pthread_detach(*handle);
    
    free(handle);
    return (ret == 0) ? INFRA_OK : INFRA_ERR_THREAD;
}

infra_error_t infra_mutex_create(infra_mutex_t* mutex) {
    if (!mutex) {
        return INFRA_ERR_PARAM;
    }

    pthread_mutex_t* handle = malloc(sizeof(pthread_mutex_t));
    if (!handle) {
        return INFRA_ERR_MEMORY;
    }

    int ret = pthread_mutex_init(handle, NULL);
    if (ret != 0) {
        free(handle);
        return INFRA_ERR_MUTEX;
    }

    *mutex = handle;
    return INFRA_OK;
}

infra_error_t infra_mutex_destroy(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERR_PARAM;
    }

    pthread_mutex_t* handle = (pthread_mutex_t*)mutex;
    int ret = pthread_mutex_destroy(handle);
    
    free(handle);
    return (ret == 0) ? INFRA_OK : INFRA_ERR_MUTEX;
}

infra_error_t infra_mutex_lock(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERR_PARAM;
    }

    pthread_mutex_t* handle = (pthread_mutex_t*)mutex;
    int ret = pthread_mutex_lock(handle);
    
    return (ret == 0) ? INFRA_OK : INFRA_ERR_MUTEX;
}

infra_error_t infra_mutex_unlock(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERR_PARAM;
    }

    pthread_mutex_t* handle = (pthread_mutex_t*)mutex;
    int ret = pthread_mutex_unlock(handle);
    
    return (ret == 0) ? INFRA_OK : INFRA_ERR_MUTEX;
}

//-----------------------------------------------------------------------------
// File System Operations Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_file_open(const char* path, int flags, int mode, int* fd) {
    if (!path || !fd) {
        return INFRA_ERR_PARAM;
    }

    *fd = open(path, flags, mode);
    return (*fd >= 0) ? INFRA_OK : INFRA_ERR_IO;
}

infra_error_t infra_file_close(int fd) {
    return (close(fd) == 0) ? INFRA_OK : INFRA_ERR_IO;
}

infra_error_t infra_file_read(int fd, void* buf, size_t count, ssize_t* nread) {
    if (!buf || !nread) {
        return INFRA_ERR_PARAM;
    }

    *nread = read(fd, buf, count);
    return (*nread >= 0) ? INFRA_OK : INFRA_ERR_IO;
}

infra_error_t infra_file_write(int fd, const void* buf, size_t count, ssize_t* nwritten) {
    if (!buf || !nwritten) {
        return INFRA_ERR_PARAM;
    }

    *nwritten = write(fd, buf, count);
    return (*nwritten >= 0) ? INFRA_OK : INFRA_ERR_IO;
}

//-----------------------------------------------------------------------------
// Time Operations Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_time_now(struct timespec* ts) {
    if (!ts) {
        return INFRA_ERR_PARAM;
    }

    return (clock_gettime(CLOCK_REALTIME, ts) == 0) ? INFRA_OK : INFRA_ERR_TIME;
}

uint64_t infra_time_monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

//-----------------------------------------------------------------------------
// System Information Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_sys_cpu_count(int* count) {
    if (!count) {
        return INFRA_ERR_PARAM;
    }

    *count = sysconf(_SC_NPROCESSORS_ONLN);
    return (*count > 0) ? INFRA_OK : INFRA_ERR_SYSTEM;
}

infra_error_t infra_sys_page_size(size_t* size) {
    if (!size) {
        return INFRA_ERR_PARAM;
    }

    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) {
        return INFRA_ERR_SYSTEM;
    }

    *size = (size_t)ps;
    return INFRA_OK;
} 