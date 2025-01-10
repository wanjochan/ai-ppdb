/*
 * infra_platform.h - Platform Abstraction Layer
 */

#ifndef PPDB_INFRA_PLATFORM_H
#define PPDB_INFRA_PLATFORM_H

#include "internal/infra/infra.h"

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------
typedef void* infra_thread_t;
typedef void* infra_mutex_t;
typedef void* infra_cond_t;
typedef void* infra_rwlock_t;

infra_error_t infra_thread_create(infra_thread_t* thread, void* (*func)(void*), void* arg);
infra_error_t infra_thread_join(infra_thread_t thread, void** retval);
infra_error_t infra_thread_detach(infra_thread_t thread);

infra_error_t infra_mutex_create(infra_mutex_t* mutex);
infra_error_t infra_mutex_destroy(infra_mutex_t mutex);
infra_error_t infra_mutex_lock(infra_mutex_t mutex);
infra_error_t infra_mutex_unlock(infra_mutex_t mutex);
infra_error_t infra_mutex_trylock(infra_mutex_t mutex);

infra_error_t infra_cond_create(infra_cond_t* cond);
infra_error_t infra_cond_destroy(infra_cond_t cond);
infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex);
infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, const struct timespec* ts);
infra_error_t infra_cond_signal(infra_cond_t cond);
infra_error_t infra_cond_broadcast(infra_cond_t cond);

infra_error_t infra_rwlock_create(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_destroy(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_rdlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_wrlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_unlock(infra_rwlock_t rwlock);

//-----------------------------------------------------------------------------
// File System Operations
//-----------------------------------------------------------------------------
infra_error_t infra_file_open(const char* path, int flags, int mode, int* fd);
infra_error_t infra_file_close(int fd);
infra_error_t infra_file_read(int fd, void* buf, size_t count, ssize_t* nread);
infra_error_t infra_file_write(int fd, const void* buf, size_t count, ssize_t* nwritten);
infra_error_t infra_file_seek(int fd, off_t offset, int whence, off_t* new_offset);
infra_error_t infra_file_sync(int fd);

//-----------------------------------------------------------------------------
// Network Operations
//-----------------------------------------------------------------------------
infra_error_t infra_socket_create(int domain, int type, int protocol, int* fd);
infra_error_t infra_socket_bind(int fd, const struct sockaddr* addr, socklen_t addrlen);
infra_error_t infra_socket_listen(int fd, int backlog);
infra_error_t infra_socket_accept(int fd, struct sockaddr* addr, socklen_t* addrlen, int* client_fd);
infra_error_t infra_socket_connect(int fd, const struct sockaddr* addr, socklen_t addrlen);

//-----------------------------------------------------------------------------
// Time Operations
//-----------------------------------------------------------------------------
infra_error_t infra_time_now(struct timespec* ts);
infra_error_t infra_time_sleep(const struct timespec* ts);
uint64_t infra_time_monotonic_ms(void);

//-----------------------------------------------------------------------------
// System Information
//-----------------------------------------------------------------------------
infra_error_t infra_sys_cpu_count(int* count);
infra_error_t infra_sys_page_size(size_t* size);
infra_error_t infra_sys_memory_info(size_t* total, size_t* available);

#endif /* PPDB_INFRA_PLATFORM_H */ 