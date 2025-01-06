#ifndef PPDB_BASE_ASYNC_IMPL_H_
#define PPDB_BASE_ASYNC_IMPL_H_

#include "base_async_common.h"

//-----------------------------------------------------------------------------
// Async Implementation Interface
//-----------------------------------------------------------------------------

// Implementation context and operations
typedef struct ppdb_base_async_impl {
    // Implementation name
    const char* name;

    // Initialize/cleanup implementation
    ppdb_error_t (*init)(void** context);
    void (*cleanup)(void* context);
    
    // Event loop operations
    ppdb_error_t (*create_loop)(void* context, ppdb_base_async_loop_t** loop);
    ppdb_error_t (*destroy_loop)(void* context, ppdb_base_async_loop_t* loop);
    ppdb_error_t (*run_loop)(void* context, ppdb_base_async_loop_t* loop, int timeout_ms);
    ppdb_error_t (*stop_loop)(void* context, ppdb_base_async_loop_t* loop);
    
    // IO handle operations
    ppdb_error_t (*create_handle)(void* context, ppdb_base_async_loop_t* loop, 
                                 int fd, ppdb_base_async_handle_t** handle);
    ppdb_error_t (*destroy_handle)(void* context, ppdb_base_async_handle_t* handle);
    ppdb_error_t (*read)(void* context, ppdb_base_async_handle_t* handle, 
                        void* buf, size_t len, ppdb_base_async_cb cb);
    ppdb_error_t (*write)(void* context, ppdb_base_async_handle_t* handle,
                         const void* buf, size_t len, ppdb_base_async_cb cb);

    // Implementation specific data
    void* impl_data;
} ppdb_base_async_impl_t;

// Get epoll implementation
const ppdb_base_async_impl_t* ppdb_base_async_get_epoll_impl(void);

// Get IOCP implementation
const ppdb_base_async_impl_t* ppdb_base_async_get_iocp_impl(void);

// Get default implementation for current platform
const ppdb_base_async_impl_t* ppdb_base_async_get_default_impl(void);

#endif // PPDB_BASE_ASYNC_IMPL_H_