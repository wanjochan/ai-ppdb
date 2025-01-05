#ifndef PPDB_INTERNAL_CORE_H
#define PPDB_INTERNAL_CORE_H

#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// Core Types
//-----------------------------------------------------------------------------
typedef struct ppdb_core_mutex ppdb_core_mutex_t;
typedef struct ppdb_core_rwlock ppdb_core_rwlock_t;
typedef struct ppdb_core_cond ppdb_core_cond_t;
typedef struct ppdb_core_file ppdb_core_file_t;
typedef struct ppdb_core_thread ppdb_core_thread_t;
typedef struct ppdb_core_async_loop ppdb_core_async_loop_t;
typedef struct ppdb_core_async_handle ppdb_core_async_handle_t;
typedef struct ppdb_core_async_future ppdb_core_async_future_t;

// Callback types
typedef void (*ppdb_core_async_cb)(ppdb_core_async_handle_t* handle, int status);

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------
#define PPDB_ALIGNMENT 64
#define PPDB_CACHELINE_SIZE 64

void* ppdb_core_alloc(size_t size);
void ppdb_core_free(void* ptr);
void* ppdb_core_calloc(size_t nmemb, size_t size);
void* ppdb_core_realloc(void* ptr, size_t size);
void* ppdb_core_aligned_alloc(size_t alignment, size_t size);

//-----------------------------------------------------------------------------
// Synchronization Primitives
//-----------------------------------------------------------------------------

// Sync types
typedef enum ppdb_core_sync_type {
    PPDB_CORE_SYNC_MUTEX,
    PPDB_CORE_SYNC_SPINLOCK,
    PPDB_CORE_SYNC_RWLOCK,
    PPDB_CORE_SYNC_LOCKFREE
} ppdb_core_sync_type_t;

// Sync configuration
typedef struct ppdb_core_sync_config {
    ppdb_core_sync_type_t type;
    bool use_lockfree;
    uint32_t spin_count;
    uint32_t timeout_ms;
} ppdb_core_sync_config_t;

// Mutex operations
ppdb_error_t ppdb_core_mutex_create(ppdb_core_mutex_t** mutex);
ppdb_error_t ppdb_core_mutex_destroy(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_lock(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_unlock(ppdb_core_mutex_t* mutex);
ppdb_error_t ppdb_core_mutex_trylock(ppdb_core_mutex_t* mutex);

// RWLock operations
ppdb_error_t ppdb_core_rwlock_create(ppdb_core_rwlock_t** lock);
ppdb_error_t ppdb_core_rwlock_destroy(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_rdlock(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_wrlock(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_unlock(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_tryrdlock(ppdb_core_rwlock_t* lock);
ppdb_error_t ppdb_core_rwlock_trywrlock(ppdb_core_rwlock_t* lock);

// Atomic operations
size_t ppdb_core_atomic_load(const size_t* ptr);
void ppdb_core_atomic_store(size_t* ptr, size_t val);
size_t ppdb_core_atomic_add(size_t* ptr, size_t val);
size_t ppdb_core_atomic_sub(size_t* ptr, size_t val);
bool ppdb_core_atomic_cas(size_t* ptr, size_t expected, size_t desired);

//-----------------------------------------------------------------------------
// Asynchronous Operations
//-----------------------------------------------------------------------------

// Async loop
ppdb_error_t ppdb_core_async_loop_create(ppdb_core_async_loop_t** loop);
ppdb_error_t ppdb_core_async_loop_destroy(ppdb_core_async_loop_t* loop);
ppdb_error_t ppdb_core_async_loop_run(ppdb_core_async_loop_t* loop, int timeout_ms);

// Async I/O
ppdb_error_t ppdb_core_async_handle_create(ppdb_core_async_loop_t* loop, 
                                          int fd, 
                                          ppdb_core_async_handle_t** handle);
ppdb_error_t ppdb_core_async_handle_destroy(ppdb_core_async_handle_t* handle);
ppdb_error_t ppdb_core_async_read(ppdb_core_async_handle_t* handle, 
                                 void* buf, 
                                 size_t len, 
                                 ppdb_core_async_cb cb);
ppdb_error_t ppdb_core_async_write(ppdb_core_async_handle_t* handle, 
                                  const void* buf, 
                                  size_t len, 
                                  ppdb_core_async_cb cb);

// Future pattern
ppdb_error_t ppdb_core_async_future_create(ppdb_core_async_loop_t* loop,
                                          ppdb_core_async_future_t** future);
ppdb_error_t ppdb_core_async_future_destroy(ppdb_core_async_future_t* future);
ppdb_error_t ppdb_core_async_future_wait(ppdb_core_async_future_t* future);
ppdb_error_t ppdb_core_async_future_is_ready(ppdb_core_async_future_t* future, bool* ready);

//-----------------------------------------------------------------------------
// File System Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_core_file_open(const char* path, const char* mode, ppdb_core_file_t** file);
ppdb_error_t ppdb_core_file_close(ppdb_core_file_t* file);
ppdb_error_t ppdb_core_file_read(ppdb_core_file_t* file, void* buf, size_t size, size_t* read);
ppdb_error_t ppdb_core_file_write(ppdb_core_file_t* file, const void* buf, size_t size, size_t* written);
ppdb_error_t ppdb_core_file_sync(ppdb_core_file_t* file);
ppdb_error_t ppdb_core_file_seek(ppdb_core_file_t* file, int64_t offset, int whence);
ppdb_error_t ppdb_core_file_tell(ppdb_core_file_t* file, int64_t* pos);

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_core_thread_create(ppdb_core_thread_t** thread, 
                                    void* (*start_routine)(void*), 
                                    void* arg);
ppdb_error_t ppdb_core_thread_join(ppdb_core_thread_t* thread, void** retval);
ppdb_error_t ppdb_core_thread_detach(ppdb_core_thread_t* thread);
ppdb_error_t ppdb_core_thread_yield(void);

#endif // PPDB_INTERNAL_CORE_H
