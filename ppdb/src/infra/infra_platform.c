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
        return INFRA_ERROR_INVALID;
    }

    pthread_t* handle = infra_malloc(sizeof(pthread_t));
    if (!handle) {
        return INFRA_ERROR_MEMORY;
    }

    if (pthread_create(handle, NULL, func, arg) != 0) {
        infra_free(handle);
        return INFRA_ERROR_SYSTEM;
    }

    *thread = handle;
    return INFRA_OK;
}

infra_error_t infra_thread_join(infra_thread_t thread, void** retval) {
    if (!thread) {
        return INFRA_ERROR_INVALID;
    }

    pthread_t* handle = (pthread_t*)thread;
    int ret = pthread_join(*handle, retval);
    infra_free(handle);
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_thread_detach(infra_thread_t thread) {
    if (!thread) {
        return INFRA_ERROR_INVALID;
    }

    pthread_t* handle = (pthread_t*)thread;
    int ret = pthread_detach(*handle);
    infra_free(handle);
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_mutex_create(infra_mutex_t* mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* handle = infra_malloc(sizeof(pthread_mutex_t));
    if (!handle) {
        return INFRA_ERROR_MEMORY;
    }

    if (pthread_mutex_init(handle, NULL) != 0) {
        infra_free(handle);
        return INFRA_ERROR_SYSTEM;
    }

    *mutex = handle;
    return INFRA_OK;
}

infra_error_t infra_mutex_destroy(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* handle = (pthread_mutex_t*)mutex;
    int ret = pthread_mutex_destroy(handle);
    infra_free(handle);
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_mutex_lock(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* handle = (pthread_mutex_t*)mutex;
    return (pthread_mutex_lock(handle) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_mutex_unlock(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* handle = (pthread_mutex_t*)mutex;
    return (pthread_mutex_unlock(handle) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_mutex_trylock(infra_mutex_t mutex) {
    if (!mutex) {
        return INFRA_ERROR_INVALID;
    }

    pthread_mutex_t* handle = (pthread_mutex_t*)mutex;
    int ret = pthread_mutex_trylock(handle);
    
    if (ret == EBUSY) {
        return INFRA_ERROR_BUSY;
    }
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_cond_create(infra_cond_t* cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* handle = infra_malloc(sizeof(pthread_cond_t));
    if (!handle) {
        return INFRA_ERROR_MEMORY;
    }

    if (pthread_cond_init(handle, NULL) != 0) {
        infra_free(handle);
        return INFRA_ERROR_SYSTEM;
    }

    *cond = handle;
    return INFRA_OK;
}

infra_error_t infra_cond_destroy(infra_cond_t cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* handle = (pthread_cond_t*)cond;
    int ret = pthread_cond_destroy(handle);
    infra_free(handle);
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex) {
    if (!cond || !mutex) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond_handle = (pthread_cond_t*)cond;
    pthread_mutex_t* mutex_handle = (pthread_mutex_t*)mutex;
    return (pthread_cond_wait(cond_handle, mutex_handle) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, const struct timespec* ts) {
    if (!cond || !mutex || !ts) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* cond_handle = (pthread_cond_t*)cond;
    pthread_mutex_t* mutex_handle = (pthread_mutex_t*)mutex;
    
    int ret = pthread_cond_timedwait(cond_handle, mutex_handle, ts);
    if (ret == ETIMEDOUT) {
        return INFRA_ERROR_TIMEOUT;
    }
    return (ret == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_cond_signal(infra_cond_t cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* handle = (pthread_cond_t*)cond;
    return (pthread_cond_signal(handle) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

infra_error_t infra_cond_broadcast(infra_cond_t cond) {
    if (!cond) {
        return INFRA_ERROR_INVALID;
    }

    pthread_cond_t* handle = (pthread_cond_t*)cond;
    return (pthread_cond_broadcast(handle) == 0) ? INFRA_OK : INFRA_ERROR_SYSTEM;
}

//-----------------------------------------------------------------------------
// File System Operations Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_file_open(const char* path, int flags, int mode, int* fd) {
    if (!path || !fd) {
        return INFRA_ERROR_INVALID;
    }

    *fd = open(path, flags, mode);
    return (*fd >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_close(int fd) {
    return (close(fd) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_read(int fd, void* buf, size_t count, ssize_t* nread) {
    if (!buf || !nread) {
        return INFRA_ERROR_INVALID;
    }

    *nread = read(fd, buf, count);
    return (*nread >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_write(int fd, const void* buf, size_t count, ssize_t* nwritten) {
    if (!buf || !nwritten) {
        return INFRA_ERROR_INVALID;
    }

    *nwritten = write(fd, buf, count);
    return (*nwritten >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_seek(int fd, off_t offset, int whence, off_t* new_offset) {
    if (!new_offset) {
        return INFRA_ERROR_INVALID;
    }

    *new_offset = lseek(fd, offset, whence);
    return (*new_offset >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_sync(int fd) {
    return (fsync(fd) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_truncate(int fd, off_t length) {
    return (ftruncate(fd, length) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_stat(const char* path, struct stat* buf) {
    if (!path || !buf) {
        return INFRA_ERROR_INVALID;
    }

    return (stat(path, buf) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_remove(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID;
    }

    return (unlink(path) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_file_rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath) {
        return INFRA_ERROR_INVALID;
    }

    return (rename(oldpath, newpath) == 0) ? INFRA_OK : INFRA_ERROR_IO;
}

//-----------------------------------------------------------------------------
// IO Operations Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_printf(const char* format, ...) {
    if (!format) {
        return INFRA_ERROR_INVALID;
    }

    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);

    return (ret >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_dprintf(int fd, const char* format, ...) {
    if (!format) {
        return INFRA_ERROR_INVALID;
    }

    va_list args;
    va_start(args, format);
    int ret = vdprintf(fd, format, args);
    va_end(args);

    return (ret >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_puts(const char* str) {
    if (!str) {
        return INFRA_ERROR_INVALID;
    }

    return (puts(str) >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

infra_error_t infra_putchar(int ch) {
    return (putchar(ch) >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

//-----------------------------------------------------------------------------
// Network Operations Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_socket_create(int domain, int type, int protocol, int* fd) {
    if (!fd) {
        return INFRA_ERROR_INVALID;
    }

    *fd = socket(domain, type, protocol);
    return (*fd >= 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_bind(int fd, const struct sockaddr* addr, socklen_t addrlen) {
    if (!addr) {
        return INFRA_ERROR_INVALID;
    }

    return (bind(fd, addr, addrlen) == 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_listen(int fd, int backlog) {
    return (listen(fd, backlog) == 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_accept(int fd, struct sockaddr* addr, socklen_t* addrlen, int* client_fd) {
    if (!client_fd) {
        return INFRA_ERROR_INVALID;
    }

    *client_fd = accept(fd, addr, addrlen);
    return (*client_fd >= 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
    if (!addr) {
        return INFRA_ERROR_INVALID;
    }

    return (connect(fd, addr, addrlen) == 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_close(int fd) {
    return (close(fd) == 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_shutdown(int fd, int how) {
    return (shutdown(fd, how) == 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_send(int fd, const void* buf, size_t len, int flags, ssize_t* nsent) {
    if (!buf || !nsent) {
        return INFRA_ERROR_INVALID;
    }

    *nsent = send(fd, buf, len, flags);
    return (*nsent >= 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_recv(int fd, void* buf, size_t len, int flags, ssize_t* nrecv) {
    if (!buf || !nrecv) {
        return INFRA_ERROR_INVALID;
    }

    *nrecv = recv(fd, buf, len, flags);
    return (*nrecv >= 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_sendto(int fd, const void* buf, size_t len, int flags,
                                 const struct sockaddr* dest_addr, socklen_t addrlen,
                                 ssize_t* nsent) {
    if (!buf || !dest_addr || !nsent) {
        return INFRA_ERROR_INVALID;
    }

    *nsent = sendto(fd, buf, len, flags, dest_addr, addrlen);
    return (*nsent >= 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_recvfrom(int fd, void* buf, size_t len, int flags,
                                   struct sockaddr* src_addr, socklen_t* addrlen,
                                   ssize_t* nrecv) {
    if (!buf || !nrecv) {
        return INFRA_ERROR_INVALID;
    }

    *nrecv = recvfrom(fd, buf, len, flags, src_addr, addrlen);
    return (*nrecv >= 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_setsockopt(int fd, int level, int optname,
                                     const void* optval, socklen_t optlen) {
    if (!optval) {
        return INFRA_ERROR_INVALID;
    }

    return (setsockopt(fd, level, optname, optval, optlen) == 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
}

infra_error_t infra_socket_getsockopt(int fd, int level, int optname,
                                     void* optval, socklen_t* optlen) {
    if (!optval || !optlen) {
        return INFRA_ERROR_INVALID;
    }

    return (getsockopt(fd, level, optname, optval, optlen) == 0) ? INFRA_OK : INFRA_ERROR_NETWORK;
} 