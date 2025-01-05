#ifndef PPDB_INTERNAL_CORE_H
#define PPDB_INTERNAL_CORE_H

#include <cosmopolitan.h>
#include "ppdb/ppdb.h"

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------
#define PPDB_ALIGNMENT 64
#define PPDB_CACHELINE_SIZE 64

void* ppdb_core_malloc(size_t size);
void* ppdb_core_calloc(size_t nmemb, size_t size);
void* ppdb_core_realloc(void* ptr, size_t size);
void ppdb_core_free(void* ptr);
void* ppdb_core_aligned_alloc(size_t alignment, size_t size);

//-----------------------------------------------------------------------------
// Core Types
//-----------------------------------------------------------------------------
// Mutex
typedef struct ppdb_core_mutex ppdb_core_mutex_t;

ppdb_error_t ppdb_core_mutex_create(ppdb_core_mutex_t** mutex);
ppdb_error_t ppdb_core_mutex_destroy(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_lock(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_unlock(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_trylock(ppdb_core_mutex_t* mutex);

// Read-Write Lock
typedef struct ppdb_core_rwlock ppdb_core_rwlock_t;

ppdb_error_t ppdb_core_rwlock_create(ppdb_core_rwlock_t** lock);
ppdb_error_t ppdb_core_rwlock_destroy(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_rdlock(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_wrlock(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_unlock(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_tryrdlock(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_trywrlock(ppdb_core_rwlock_t* lock);

// Condition Variable
typedef struct ppdb_core_cond ppdb_core_cond_t;

ppdb_error_t ppdb_core_cond_create(ppdb_core_cond_t** cond);
ppdb_error_t ppdb_core_cond_destroy(ppdb_core_cond_t* cond);
ppdb_error_t ppdb_core_cond_wait(ppdb_core_cond_t* cond, ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_cond_timedwait(ppdb_core_cond_t* cond, ppdb_core_mutex_t* mutex, uint32_t timeout_ms);
ppdb_error_t ppdb_core_cond_signal(ppdb_core_cond_t* cond);
ppdb_error_t ppdb_core_cond_broadcast(ppdb_core_cond_t* cond);

// Async I/O
typedef struct ppdb_core_async_loop ppdb_core_async_loop_t;
typedef struct ppdb_core_async_handle ppdb_core_async_handle_t;
typedef struct ppdb_core_async_future ppdb_core_async_future_t;

typedef void (*ppdb_core_async_callback_t)(ppdb_core_async_handle_t* handle, ppdb_error_t status);

ppdb_error_t ppdb_core_async_loop_create(ppdb_core_async_loop_t** loop);
ppdb_error_t ppdb_core_async_loop_destroy(ppdb_core_async_loop_t* loop);
ppdb_error_t ppdb_core_async_loop_run(ppdb_core_async_loop_t* loop, int timeout_ms);

ppdb_error_t ppdb_core_async_handle_create(ppdb_core_async_loop_t* loop,
                                         ppdb_core_async_handle_t** handle);
ppdb_error_t ppdb_core_async_handle_destroy(ppdb_core_async_handle_t* handle);
ppdb_error_t ppdb_core_async_read(ppdb_core_async_handle_t* handle,
                                void* buf,
                                size_t size,
                                ppdb_core_async_callback_t callback);
ppdb_error_t ppdb_core_async_write(ppdb_core_async_handle_t* handle,
                                 const void* buf,
                                 size_t size,
                                 ppdb_core_async_callback_t callback);

ppdb_error_t ppdb_core_async_future_create(ppdb_core_async_loop_t* loop,
                                        ppdb_core_async_future_t** future);
ppdb_error_t ppdb_core_async_future_destroy(ppdb_core_async_future_t* future);
ppdb_error_t ppdb_core_async_future_wait(ppdb_core_async_future_t* future);
ppdb_error_t ppdb_core_async_future_is_ready(ppdb_core_async_future_t* future, bool* ready);

// File I/O
typedef struct ppdb_core_file ppdb_core_file_t;

ppdb_error_t ppdb_core_file_open(const char* path, const char* mode, ppdb_core_file_t** file);
ppdb_error_t ppdb_core_file_close(ppdb_core_file_t* file);
ppdb_error_t ppdb_core_file_read(ppdb_core_file_t* file, void* buf, size_t size, size_t* read);
ppdb_error_t ppdb_core_file_write(ppdb_core_file_t* file, const void* buf, size_t size, size_t* written);
ppdb_error_t ppdb_core_file_sync(ppdb_core_file_t* file);
ppdb_error_t ppdb_core_file_seek(ppdb_core_file_t* file, int64_t offset, int whence);
ppdb_error_t ppdb_core_file_tell(ppdb_core_file_t* file, int64_t* pos);

// Thread
typedef struct ppdb_core_thread ppdb_core_thread_t;
typedef void* (*ppdb_core_thread_func_t)(void* arg);

ppdb_error_t ppdb_core_thread_create(ppdb_core_thread_t** thread,
                                  ppdb_core_thread_func_t func,
                                  void* arg);
ppdb_error_t ppdb_core_thread_join(ppdb_core_thread_t* thread, void** retval);
ppdb_error_t ppdb_core_thread_detach(ppdb_core_thread_t* thread);
ppdb_error_t ppdb_core_thread_yield(void);

#endif // PPDB_INTERNAL_CORE_H
