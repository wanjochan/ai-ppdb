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

#endif /* INFRA_PLATFORM_H */ 
