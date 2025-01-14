/*
 * infra_platform.c - Platform Infrastructure Layer Implementation
 */

#include "cosmopolitan.h"
#include "infra_platform.h"
#include "infra_error.h"
#include "infra_mux.h"
#include "internal/infra/infra_core.h"

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

//已成，不用改
infra_error_t infra_platform_get_time(infra_time_t* time) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    *time = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return INFRA_OK;
}

//已成，不用改
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


//-----------------------------------------------------------------------------
// IOCP Functions
//-----------------------------------------------------------------------------

//void* infra_platform_create_iocp(void) {
//    if (!g_infra.platform.is_windows) {
//        return NULL;
//    }
//    void* iocp = (void*)CreateIoCompletionPort((int64_t)-1, 0, 0, 0);
//    if (!iocp) {
//        return NULL;
//    }
//    return iocp;
//}

//void infra_platform_close_iocp(void* iocp) {
//    if (iocp) {
//        CloseHandle((int64_t)iocp);
//    }
//}

//infra_error_t infra_platform_iocp_add(void* iocp, infra_socket_t sock, void* user_data) {
//    if (!iocp || !sock) {
//        return INFRA_ERROR_INVALID_PARAM;
//    }
//    
//    int64_t result = CreateIoCompletionPort((int64_t)sock->handle, (int64_t)iocp, (uint64_t)user_data, 0);
//    if (!result) {
//        int error = GetLastError();
//        infra_printf("CreateIoCompletionPort failed with error: %d\n", error);
//        return INFRA_ERROR_SYSTEM;
//    }
//    
//    return INFRA_OK;
//}

//infra_error_t infra_platform_iocp_wait(void* iocp, void* events, size_t max_events, int timeout_ms) {
//    if (!iocp || !events || max_events == 0) {
//        return INFRA_ERROR_INVALID_PARAM;
//    }
//
//    uint32_t bytes;
//    uint64_t key;
//    struct NtOverlapped* overlapped = NULL;
//    
//    if (!GetQueuedCompletionStatus((int64_t)iocp, &bytes, &key, &overlapped, timeout_ms)) {
//        int err = GetLastError();
//        if (err == WSA_WAIT_TIMEOUT) {
//            return 0;
//        }
//        return INFRA_ERROR_SYSTEM;
//    }
//    
//    infra_mux_event_t* mux_events = (infra_mux_event_t*)events;
//    mux_events[0].user_data = (void*)key;
//    mux_events[0].events = INFRA_EVENT_READ | INFRA_EVENT_WRITE;
//    return 1;
//}

//-----------------------------------------------------------------------------
// EPOLL Functions
//-----------------------------------------------------------------------------

int infra_platform_create_epoll(void) {
    return epoll_create1(0);
}

void infra_platform_close_epoll(int epoll_fd) {
    if (epoll_fd >= 0) {
        close(epoll_fd);
    }
}
/*
关于 infra_platform_epoll_add 中未使用的 user_data 参数：
这不是设计问题，而是一个有意为之的设计。从代码中可以看到：
在 Linux epoll 中，epoll_event 结构体有一个 data 字段，可以用来存储用户数据
目前的实现只使用了 data.fd 来存储文件描述符
user_data 参数的存在是为了保持与 Windows IOCP 的接口一致性（可以看到注释掉的 IOCP 相关函数也有 user_data 参数）
这个参数在将来可能会用于存储更多的上下文信息，比如回调函数、连接状态等
所以这个未使用的参数是一个预留的扩展点，用于：
保持与 Windows IOCP 的接口一致性
为将来可能的功能扩展预留空间
允许用户在事件触发时能够获取到相关的上下文信息
*/
infra_error_t infra_platform_epoll_add(int epoll_fd, int fd, int events, bool edge_trigger, void* user_data) {
    /* user_data 参数预留给将来扩展使用，比如存储回调函数或上下文信息 */
    (void)user_data;  // 显式忽略未使用的参数

    if (epoll_fd < 0 || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    struct epoll_event ev = {0};
    ev.events = ((events & INFRA_EVENT_READ) ? EPOLLIN : 0) |
                ((events & INFRA_EVENT_WRITE) ? EPOLLOUT : 0) |
                ((events & INFRA_EVENT_ERROR) ? EPOLLERR : 0) |
                (edge_trigger ? EPOLLET : 0);
    ev.data.fd = fd;
    
    infra_printf("Adding fd %d to epoll with events 0x%x\n", fd, ev.events);
    return (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_epoll_modify(int epoll_fd, int fd, int events, bool edge_trigger) {
    if (epoll_fd < 0 || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    struct epoll_event ev = {0};
    ev.events = ((events & INFRA_EVENT_READ) ? EPOLLIN : 0) |
                ((events & INFRA_EVENT_WRITE) ? EPOLLOUT : 0) |
                ((events & INFRA_EVENT_ERROR) ? EPOLLERR : 0) |
                (edge_trigger ? EPOLLET : 0);
    ev.data.fd = fd;
    
    infra_printf("Modifying fd %d in epoll with events 0x%x\n", fd, ev.events);
    return (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_epoll_remove(int epoll_fd, int fd) {
    if (epoll_fd < 0 || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    infra_printf("Removing fd %d from epoll\n", fd);
    return (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_platform_epoll_wait(int epoll_fd, void* events, size_t max_events, int timeout_ms) {
    if (epoll_fd < 0 || !events || max_events == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    int ret;
    do {
        ret = epoll_wait(epoll_fd, (struct epoll_event*)events, max_events, timeout_ms);
        if (ret > 0) {
            struct epoll_event* ev = (struct epoll_event*)events;
            for (int i = 0; i < ret; i++) {
                infra_printf("epoll_wait: fd %d has events 0x%x\n", ev[i].data.fd, ev[i].events);
            }
        }
    } while (ret < 0 && errno == EINTR);  // 如果被信号中断，则重试

    if (ret < 0) {
        infra_printf("epoll_wait failed with errno %d\n", errno);
        return INFRA_ERROR_SYSTEM;
    }

    return ret;  // 返回事件数量
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
