/*
 * infra_platform.c - Platform Infrastructure Layer Implementation
 */

#include "cosmopolitan.h"
#include "infra_platform.h"
#include "infra_error.h"
#include "infra_mux.h"

// 事件类型定义
#define INFRA_EVENT_READ  1
#define INFRA_EVENT_WRITE 2
#define INFRA_EVENT_ERROR 4

//-----------------------------------------------------------------------------
// Platform Functions
//-----------------------------------------------------------------------------

infra_error_t infra_platform_init(void) {
    return INFRA_OK;
}

infra_error_t infra_platform_get_pid(infra_pid_t* pid) {
    *pid = getpid();
    return INFRA_OK;
}

infra_error_t infra_platform_get_tid(infra_tid_t* tid) {
    //*tid = gettid();
    *tid = pthread_self();//@cosmopolitan
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
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    *time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return INFRA_OK;
}

infra_error_t infra_platform_get_monotonic_time(infra_time_t* time) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

infra_error_t infra_platform_thread_create(void** handle, infra_thread_func_t func, void* arg) {
    pthread_t* thread = malloc(sizeof(pthread_t));
    if (pthread_create(thread, NULL, func, arg) != 0) {
        free(thread);
        return INFRA_ERROR_SYSTEM;
    }
    *handle = thread;
    return INFRA_OK;
}

infra_error_t infra_platform_thread_join(void* handle) {
    pthread_t* thread = (pthread_t*)handle;
    if (pthread_join(*thread, NULL) != 0) {
        return INFRA_ERROR_SYSTEM;
    }
    free(thread);
    return INFRA_OK;
}

infra_error_t infra_platform_thread_detach(void* handle) {
    pthread_t* thread = (pthread_t*)handle;
    if (pthread_detach(*thread) != 0) {
        return INFRA_ERROR_SYSTEM;
    }
    free(thread);
    return INFRA_OK;
}

void infra_platform_thread_exit(void* retval) {
    pthread_exit(retval);
}

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_mutex_create(void** handle) {
    pthread_mutex_t* mutex = malloc(sizeof(pthread_mutex_t));
    if (pthread_mutex_init(mutex, NULL) != 0) {
        free(mutex);
        return INFRA_ERROR_SYSTEM;
    }
    *handle = mutex;
    return INFRA_OK;
}

void infra_platform_mutex_destroy(void* handle) {
    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    pthread_mutex_destroy(mutex);
    free(mutex);
}

infra_error_t infra_platform_mutex_lock(void* handle) {
    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    return (pthread_mutex_lock(mutex) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_mutex_trylock(void* handle) {
    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    int ret = pthread_mutex_trylock(mutex);
    return (ret == 0) ? INFRA_OK : (ret == EBUSY ? INFRA_ERROR_BUSY : INFRA_ERROR_SYSTEM);
}

infra_error_t infra_platform_mutex_unlock(void* handle) {
    pthread_mutex_t* mutex = (pthread_mutex_t*)handle;
    return (pthread_mutex_unlock(mutex) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

//-----------------------------------------------------------------------------
// Platform Detection
//-----------------------------------------------------------------------------

bool infra_platform_is_windows(void) {
    return IsWindows();
}

//-----------------------------------------------------------------------------
// IOCP Functions
//-----------------------------------------------------------------------------

void* infra_platform_create_iocp(void) {
    return (void*)CreateIoCompletionPort((int64_t)-1, 0, 0, 0);
}

void infra_platform_close_iocp(void* iocp) {
    CloseHandle((int64_t)iocp);
}

infra_error_t infra_platform_iocp_add(void* iocp, int fd, void* user_data) {
    int64_t result = CreateIoCompletionPort((int64_t)fd, (int64_t)iocp, (uint64_t)user_data, 0);
    return result ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_iocp_wait(void* iocp, void* events, size_t max_events, int timeout_ms) {
    uint32_t bytes;
    uint64_t key;
    struct NtOverlapped* overlapped = NULL;
    
    if (!GetQueuedCompletionStatus((int64_t)iocp, &bytes, &key, &overlapped, timeout_ms)) {
        return GetLastError() == WSA_WAIT_TIMEOUT //@cosmopolitan
		? 0 : INFRA_ERROR_SYSTEM;
    }
    
    if (events && max_events > 0) {
        infra_mux_event_t* mux_events = (infra_mux_event_t*)events;
        mux_events[0].user_data = (void*)key;
        mux_events[0].events = INFRA_EVENT_READ | INFRA_EVENT_WRITE;
        return 1;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// EPOLL Functions
//-----------------------------------------------------------------------------

int infra_platform_create_epoll(void) {
    return epoll_create1(0);
}

void infra_platform_close_epoll(int epoll_fd) {
    close(epoll_fd);
}

infra_error_t infra_platform_epoll_add(int epoll_fd, int fd, int events, bool edge_trigger, void* user_data) {
    struct epoll_event ev = {0};
    ev.events = ((events & INFRA_EVENT_READ) ? EPOLLIN : 0) |
                ((events & INFRA_EVENT_WRITE) ? EPOLLOUT : 0) |
                ((events & INFRA_EVENT_ERROR) ? EPOLLERR : 0) |
                (edge_trigger ? EPOLLET : 0);
    ev.data.ptr = user_data;
    
    return (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_epoll_modify(int epoll_fd, int fd, int events, bool edge_trigger) {
    struct epoll_event ev = {0};
    ev.events = ((events & INFRA_EVENT_READ) ? EPOLLIN : 0) |
                ((events & INFRA_EVENT_WRITE) ? EPOLLOUT : 0) |
                ((events & INFRA_EVENT_ERROR) ? EPOLLERR : 0) |
                (edge_trigger ? EPOLLET : 0);
    
    return (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_epoll_remove(int epoll_fd, int fd) {
    return (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_epoll_wait(int epoll_fd, void* events, size_t max_events, int timeout_ms) {
    int num_events = epoll_wait(epoll_fd, (struct epoll_event*)events, max_events, timeout_ms);
    return (num_events >= 0) ? num_events : (errno == EINTR ? 0 : INFRA_ERROR_SYSTEM);
}

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_cond_create(void** handle) {
    pthread_cond_t* cond = malloc(sizeof(pthread_cond_t));
    if (pthread_cond_init(cond, NULL) != 0) {
        free(cond);
        return INFRA_ERROR_SYSTEM;
    }
    *handle = cond;
    return INFRA_OK;
}

void infra_platform_cond_destroy(void* handle) {
    pthread_cond_t* cond = (pthread_cond_t*)handle;
    pthread_cond_destroy(cond);
    free(cond);
}

infra_error_t infra_platform_cond_wait(void* handle, void* mutex) {
    pthread_cond_t* cond = (pthread_cond_t*)handle;
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;
    return (pthread_cond_wait(cond, mtx) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_cond_timedwait(void* handle, void* mutex, uint64_t timeout_ms) {
    pthread_cond_t* cond = (pthread_cond_t*)handle;
    pthread_mutex_t* mtx = (pthread_mutex_t*)mutex;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    int ret = pthread_cond_timedwait(cond, mtx, &ts);
    return (ret == 0) ? INFRA_OK : (ret == ETIMEDOUT ? INFRA_ERROR_TIMEOUT : INFRA_ERROR_SYSTEM);
}

infra_error_t infra_platform_cond_signal(void* handle) {
    pthread_cond_t* cond = (pthread_cond_t*)handle;
    return (pthread_cond_signal(cond) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_cond_broadcast(void* handle) {
    pthread_cond_t* cond = (pthread_cond_t*)handle;
    return (pthread_cond_broadcast(cond) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

infra_error_t infra_platform_rwlock_create(void** handle) {
    pthread_rwlock_t* rwlock = malloc(sizeof(pthread_rwlock_t));
    if (pthread_rwlock_init(rwlock, NULL) != 0) {
        free(rwlock);
        return INFRA_ERROR_SYSTEM;
    }
    *handle = rwlock;
    return INFRA_OK;
}

void infra_platform_rwlock_destroy(void* handle) {
    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    pthread_rwlock_destroy(rwlock);
    free(rwlock);
}

infra_error_t infra_platform_rwlock_rdlock(void* handle) {
    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_rdlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_rwlock_tryrdlock(void* handle) {
    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    int ret = pthread_rwlock_tryrdlock(rwlock);
    return (ret == 0) ? INFRA_OK : (ret == EBUSY ? INFRA_ERROR_BUSY : INFRA_ERROR_SYSTEM);
}

infra_error_t infra_platform_rwlock_wrlock(void* handle) {
    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_wrlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_rwlock_trywrlock(void* handle) {
    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    int ret = pthread_rwlock_trywrlock(rwlock);
    return (ret == 0) ? INFRA_OK : (ret == EBUSY ? INFRA_ERROR_BUSY : INFRA_ERROR_SYSTEM);
}

infra_error_t infra_platform_rwlock_unlock(void* handle) {
    pthread_rwlock_t* rwlock = (pthread_rwlock_t*)handle;
    return (pthread_rwlock_unlock(rwlock) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
} 
