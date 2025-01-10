/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_platform.h - Platform Abstraction Layer
 */

#ifndef INFRA_PLATFORM_H
#define INFRA_PLATFORM_H

// Basic types
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef long long i64;
typedef int i32;
typedef short i16;
typedef char i8;

// Common platform-independent types
typedef int infra_error_t;
typedef unsigned int infra_flags_t;

// Event types (from cosmopolitan)
#define INFRA_EVENT_READ  0x1
#define INFRA_EVENT_WRITE 0x2

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
infra_error_t infra_cond_signal(infra_cond_t cond);
infra_error_t infra_cond_broadcast(infra_cond_t cond);

infra_error_t infra_rwlock_create(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_destroy(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_rdlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_wrlock(infra_rwlock_t rwlock);
infra_error_t infra_rwlock_unlock(infra_rwlock_t rwlock);

//-----------------------------------------------------------------------------
// System Information
//-----------------------------------------------------------------------------
infra_error_t infra_sys_cpu_count(int* count);
infra_error_t infra_sys_page_size(size_t* size);
infra_error_t infra_sys_memory_info(size_t* total, size_t* available);
infra_error_t infra_sys_cpu_features(uint32_t* features);
infra_error_t infra_sys_hostname(char* name, size_t size);
infra_error_t infra_sys_username(char* name, size_t size);

#endif /* INFRA_PLATFORM_H */ 
