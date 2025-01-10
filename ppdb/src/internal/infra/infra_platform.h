/*
 * infra_platform.h - Platform Abstraction Layer
 */

#ifndef INFRA_PLATFORM_H
#define INFRA_PLATFORM_H

#include "internal/infra/infra.h"

//-----------------------------------------------------------------------------
// Platform Detection
//-----------------------------------------------------------------------------

#if defined(_WIN32) || defined(_WIN64)
    #define INFRA_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define INFRA_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define INFRA_PLATFORM_MACOS
#else
    #error "Unsupported platform"
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #define INFRA_ARCH_X64
#elif defined(__i386__) || defined(_M_IX86)
    #define INFRA_ARCH_X86
#elif defined(__arm__) || defined(_M_ARM)
    #define INFRA_ARCH_ARM
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define INFRA_ARCH_ARM64
#else
    #error "Unsupported architecture"
#endif

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

/* File open flags */
#define INFRA_O_RDONLY    0x0000
#define INFRA_O_WRONLY    0x0001
#define INFRA_O_RDWR      0x0002
#define INFRA_O_APPEND    0x0008
#define INFRA_O_CREAT     0x0200
#define INFRA_O_TRUNC     0x0400
#define INFRA_O_EXCL      0x0800
#define INFRA_O_SYNC      0x2000
#define INFRA_O_NONBLOCK  0x4000

/* File permissions */
#define INFRA_S_IRUSR  0400
#define INFRA_S_IWUSR  0200
#define INFRA_S_IXUSR  0100
#define INFRA_S_IRGRP  0040
#define INFRA_S_IWGRP  0020
#define INFRA_S_IXGRP  0010
#define INFRA_S_IROTH  0004
#define INFRA_S_IWOTH  0002
#define INFRA_S_IXOTH  0001

/* File seek origins */
#define INFRA_SEEK_SET  0
#define INFRA_SEEK_CUR  1
#define INFRA_SEEK_END  2

infra_error_t infra_file_open(const char* path, int flags, int mode, int* fd);
infra_error_t infra_file_close(int fd);
infra_error_t infra_file_read(int fd, void* buf, size_t count, ssize_t* nread);
infra_error_t infra_file_write(int fd, const void* buf, size_t count, ssize_t* nwritten);
infra_error_t infra_file_seek(int fd, off_t offset, int whence, off_t* new_offset);
infra_error_t infra_file_sync(int fd);
infra_error_t infra_file_truncate(int fd, off_t length);
infra_error_t infra_file_stat(const char* path, struct stat* buf);
infra_error_t infra_file_remove(const char* path);
infra_error_t infra_file_rename(const char* oldpath, const char* newpath);

//-----------------------------------------------------------------------------
// IO Operations
//-----------------------------------------------------------------------------

infra_error_t infra_printf(const char* format, ...);
infra_error_t infra_dprintf(int fd, const char* format, ...);
infra_error_t infra_puts(const char* str);
infra_error_t infra_putchar(int ch);
infra_error_t infra_io_read(int fd, void* buf, size_t count);
infra_error_t infra_io_write(int fd, const void* buf, size_t count);

//-----------------------------------------------------------------------------
// Network Operations
//-----------------------------------------------------------------------------

/* Socket types */
#define INFRA_SOCK_STREAM    1
#define INFRA_SOCK_DGRAM     2
#define INFRA_SOCK_NONBLOCK  0x800

/* Protocol families */
#define INFRA_AF_UNSPEC     0
#define INFRA_AF_INET       2
#define INFRA_AF_INET6      10

/* Socket options */
#define INFRA_SO_REUSEADDR  2
#define INFRA_SO_KEEPALIVE  9
#define INFRA_SO_BROADCAST  6
#define INFRA_SO_LINGER     13
#define INFRA_SO_SNDBUF     7
#define INFRA_SO_RCVBUF     8
#define INFRA_SO_SNDTIMEO   21
#define INFRA_SO_RCVTIMEO   20

infra_error_t infra_socket_create(int domain, int type, int protocol, int* fd);
infra_error_t infra_socket_bind(int fd, const struct sockaddr* addr, socklen_t addrlen);
infra_error_t infra_socket_listen(int fd, int backlog);
infra_error_t infra_socket_accept(int fd, struct sockaddr* addr, socklen_t* addrlen, int* client_fd);
infra_error_t infra_socket_connect(int fd, const struct sockaddr* addr, socklen_t addrlen);
infra_error_t infra_socket_close(int fd);
infra_error_t infra_socket_shutdown(int fd, int how);
infra_error_t infra_socket_send(int fd, const void* buf, size_t len, int flags, ssize_t* nsent);
infra_error_t infra_socket_recv(int fd, void* buf, size_t len, int flags, ssize_t* nrecv);
infra_error_t infra_socket_sendto(int fd, const void* buf, size_t len, int flags,
                                 const struct sockaddr* dest_addr, socklen_t addrlen,
                                 ssize_t* nsent);
infra_error_t infra_socket_recvfrom(int fd, void* buf, size_t len, int flags,
                                   struct sockaddr* src_addr, socklen_t* addrlen,
                                   ssize_t* nrecv);
infra_error_t infra_socket_setsockopt(int fd, int level, int optname,
                                     const void* optval, socklen_t optlen);
infra_error_t infra_socket_getsockopt(int fd, int level, int optname,
                                     void* optval, socklen_t* optlen);

//-----------------------------------------------------------------------------
// Time Operations
//-----------------------------------------------------------------------------

infra_error_t infra_time_now(struct timespec* ts);
infra_error_t infra_time_sleep(const struct timespec* ts);
uint64_t infra_time_monotonic_ms(void);
infra_error_t infra_time_format(char* buf, size_t buflen, const char* format,
                               const struct tm* tm);
infra_error_t infra_time_parse(const char* buf, const char* format,
                              struct tm* tm);

//-----------------------------------------------------------------------------
// System Information
//-----------------------------------------------------------------------------

/* CPU features */
#define INFRA_CPU_FEATURE_SSE    0x00000001
#define INFRA_CPU_FEATURE_SSE2   0x00000002
#define INFRA_CPU_FEATURE_SSE3   0x00000004
#define INFRA_CPU_FEATURE_SSSE3  0x00000008
#define INFRA_CPU_FEATURE_SSE4_1 0x00000010
#define INFRA_CPU_FEATURE_SSE4_2 0x00000020
#define INFRA_CPU_FEATURE_AVX    0x00000040
#define INFRA_CPU_FEATURE_AVX2   0x00000080
#define INFRA_CPU_FEATURE_FMA3   0x00000100
#define INFRA_CPU_FEATURE_AES    0x00000200
#define INFRA_CPU_FEATURE_SHA    0x00000400

infra_error_t infra_sys_cpu_count(int* count);
infra_error_t infra_sys_page_size(size_t* size);
infra_error_t infra_sys_memory_info(size_t* total, size_t* available);
infra_error_t infra_sys_cpu_features(uint32_t* features);
infra_error_t infra_sys_hostname(char* name, size_t size);
infra_error_t infra_sys_username(char* name, size_t size);
infra_error_t infra_sys_pid(pid_t* pid);
infra_error_t infra_sys_tid(pid_t* tid);
infra_error_t infra_sys_uid(uid_t* uid);
infra_error_t infra_sys_gid(gid_t* gid);

#endif /* INFRA_PLATFORM_H */ 