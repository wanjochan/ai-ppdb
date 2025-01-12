/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_platform.h - Platform Abstraction Layer
 */

#ifndef INFRA_PLATFORM_H
#define INFRA_PLATFORM_H

#include "internal/infra/infra_core.h"

//-----------------------------------------------------------------------------
// Platform Types
//-----------------------------------------------------------------------------

typedef uint32_t infra_pid_t;
typedef uint32_t infra_tid_t;

//-----------------------------------------------------------------------------
// Platform Functions
//-----------------------------------------------------------------------------

// 初始化平台
infra_error_t infra_platform_init(void);

// 进程和线程
infra_error_t infra_platform_get_pid(infra_pid_t* pid);
infra_error_t infra_platform_get_tid(infra_tid_t* tid);

// 时间管理
infra_error_t infra_platform_sleep(uint32_t ms);
infra_error_t infra_platform_yield(void);

// 时间获取
infra_error_t infra_platform_get_time(infra_time_t* time);
infra_error_t infra_platform_get_monotonic_time(infra_time_t* time);

// 平台相关的函数
// void infra_sleep(uint32_t ms);  // 已在infra_sync.h中定义

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

// 线程创建和管理
typedef void* (*infra_thread_func_t)(void*);
infra_error_t infra_platform_thread_create(void** handle, infra_thread_func_t func, void* arg);
infra_error_t infra_platform_thread_join(void* handle);
infra_error_t infra_platform_thread_detach(void* handle);
void infra_platform_thread_exit(void* retval);

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

// 互斥锁操作
infra_error_t infra_platform_mutex_create(void** handle);
void infra_platform_mutex_destroy(void* handle);
infra_error_t infra_platform_mutex_lock(void* handle);
infra_error_t infra_platform_mutex_trylock(void* handle);
infra_error_t infra_platform_mutex_unlock(void* handle);

//-----------------------------------------------------------------------------
// Condition Variable Operations
//-----------------------------------------------------------------------------

// 条件变量操作
infra_error_t infra_platform_cond_create(void** handle);
void infra_platform_cond_destroy(void* handle);
infra_error_t infra_platform_cond_wait(void* cond_handle, void* mutex_handle);
infra_error_t infra_platform_cond_timedwait(void* cond_handle, void* mutex_handle, uint64_t timeout_ms);
infra_error_t infra_platform_cond_signal(void* handle);
infra_error_t infra_platform_cond_broadcast(void* handle);

//-----------------------------------------------------------------------------
// Read-Write Lock Operations
//-----------------------------------------------------------------------------

// 读写锁操作
infra_error_t infra_platform_rwlock_create(void** handle);
void infra_platform_rwlock_destroy(void* handle);
infra_error_t infra_platform_rwlock_rdlock(void* handle);
infra_error_t infra_platform_rwlock_tryrdlock(void* handle);
infra_error_t infra_platform_rwlock_wrlock(void* handle);
infra_error_t infra_platform_rwlock_trywrlock(void* handle);
infra_error_t infra_platform_rwlock_unlock(void* handle);

//-----------------------------------------------------------------------------
// Byte Order Operations
//-----------------------------------------------------------------------------

uint16_t infra_htons(uint16_t host16);
uint32_t infra_htonl(uint32_t host32);
uint64_t infra_htonll(uint64_t host64);
uint16_t infra_ntohs(uint16_t net16);
uint32_t infra_ntohl(uint32_t net32);
uint64_t infra_ntohll(uint64_t net64);

//-----------------------------------------------------------------------------
// Time Management
//-----------------------------------------------------------------------------

infra_time_t infra_time_now(void);
infra_time_t infra_time_monotonic(void);
void infra_time_sleep(uint32_t ms);
void infra_time_yield(void);

//-----------------------------------------------------------------------------
// File Operations
//-----------------------------------------------------------------------------

#define INFRA_FILE_CREATE  (1 << 0)
#define INFRA_FILE_RDONLY  (1 << 1)
#define INFRA_FILE_WRONLY  (1 << 2)
#define INFRA_FILE_RDWR    (INFRA_FILE_RDONLY | INFRA_FILE_WRONLY)
#define INFRA_FILE_APPEND  (1 << 3)
#define INFRA_FILE_TRUNC   (1 << 4)

#define INFRA_SEEK_SET 0
#define INFRA_SEEK_CUR 1
#define INFRA_SEEK_END 2

infra_error_t infra_file_open(const char* path, infra_flags_t flags, int mode, INFRA_CORE_Handle_t* handle);
infra_error_t infra_file_close(INFRA_CORE_Handle_t handle);
infra_error_t infra_file_read(INFRA_CORE_Handle_t handle, void* buffer, size_t size, size_t* bytes_read);
infra_error_t infra_file_write(INFRA_CORE_Handle_t handle, const void* buffer, size_t size, size_t* bytes_written);
infra_error_t infra_file_seek(INFRA_CORE_Handle_t handle, int64_t offset, int whence);
infra_error_t infra_file_size(INFRA_CORE_Handle_t handle, size_t* size);
infra_error_t infra_file_remove(const char* path);
infra_error_t infra_file_rename(const char* old_path, const char* new_path);
infra_error_t infra_file_exists(const char* path, bool* exists);

//-----------------------------------------------------------------------------
// Atomic Operations
//-----------------------------------------------------------------------------

typedef struct {
    volatile int32_t value;
} infra_atomic_t;

void infra_atomic_init(infra_atomic_t* atomic, int32_t value);
int32_t infra_atomic_get(infra_atomic_t* atomic);
void infra_atomic_set(infra_atomic_t* atomic, int32_t value);
int32_t infra_atomic_add(infra_atomic_t* atomic, int32_t value);
int32_t infra_atomic_sub(infra_atomic_t* atomic, int32_t value);
int32_t infra_atomic_inc(infra_atomic_t* atomic);
int32_t infra_atomic_dec(infra_atomic_t* atomic);
bool infra_atomic_cas(infra_atomic_t* atomic, int32_t expected, int32_t desired);

// 平台检测函数
bool infra_platform_is_windows(void);

// IOCP相关函数
void* infra_platform_create_iocp(void);
void infra_platform_close_iocp(void* iocp);
infra_error_t infra_platform_iocp_add(void* iocp, int fd, void* user_data);
infra_error_t infra_platform_iocp_wait(void* iocp, void* events, size_t max_events, int timeout_ms);

// EPOLL相关函数
int infra_platform_create_epoll(void);
void infra_platform_close_epoll(int epoll_fd);
infra_error_t infra_platform_epoll_add(int epoll_fd, int fd, int events, bool edge_trigger, void* user_data);
infra_error_t infra_platform_epoll_modify(int epoll_fd, int fd, int events, bool edge_trigger);
infra_error_t infra_platform_epoll_remove(int epoll_fd, int fd);
infra_error_t infra_platform_epoll_wait(int epoll_fd, void* events, size_t max_events, int timeout_ms);

#endif /* INFRA_PLATFORM_H */ 
