#ifndef PPDB_ASYNC_H
#define PPDB_ASYNC_H
#include <cosmopolitan.h>

/*
 * Asynchronous I/O primitives for PPDB
 * 
 * Note: Currently using poll() for I/O multiplexing.
 * TODO: Consider optimizing for Windows IOCP in the future
 * if cosmopolitan adds support for it.
 * 
 * Design considerations for sync-async interoperability:
 * 
 * 1. Future Pattern
 *    - Converting sync operations to async (async_from_sync)
 *    - Waiting for async operations when needed (async_wait)
 *    - Allowing gradual migration from sync to async
 * 
 * 2. Resource Sharing
 *    - Mutex mechanisms for shared resources
 *    - Safe access from both sync and async contexts
 * 
 * 3. Mixed Mode Support
 *    - Embedding event loops in sync operations
 *    - Running async operations in sync contexts
 * 
 * These features will be implemented incrementally as needed.
 */

// Forward declarations
typedef struct async_loop_s async_loop_t;
typedef struct async_handle_s async_handle_t;
typedef struct async_future_s async_future_t;
typedef struct async_mutex_s async_mutex_t;
typedef void (*async_cb)(async_handle_t* handle, int status);

// Core event loop API
async_loop_t* async_loop_new(void);
void async_loop_free(async_loop_t* loop);
int async_loop_run(async_loop_t* loop, int timeout_ms);

// I/O operations
async_handle_t* async_handle_new(async_loop_t* loop, int fd);
void async_handle_free(async_handle_t* handle);
int async_handle_read(async_handle_t* handle, void* buf, size_t len, async_cb cb);

// Sync-Async interop
async_future_t* async_from_sync(async_loop_t* loop, void* sync_op);
int async_wait(async_future_t* future);
int sync_from_async(async_handle_t* handle);

// Resource sharing
async_mutex_t* async_mutex_new(void);
void async_mutex_free(async_mutex_t* mutex);
int async_mutex_lock(async_mutex_t* mutex);
int async_mutex_unlock(async_mutex_t* mutex);

// Mixed mode support
int sync_with_async(void* sync_op, async_loop_t* loop);

#endif // PPDB_ASYNC_H
