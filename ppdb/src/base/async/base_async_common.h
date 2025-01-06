#ifndef PPDB_BASE_ASYNC_COMMON_H_
#define PPDB_BASE_ASYNC_COMMON_H_

#include "internal/base.h"

//-----------------------------------------------------------------------------
// Common Async Types and Definitions
//-----------------------------------------------------------------------------

// Forward declarations
typedef struct ppdb_base_async_loop ppdb_base_async_loop_t;
typedef struct ppdb_base_async_handle ppdb_base_async_handle_t;

// Callback type for async operations
typedef void (*ppdb_base_async_cb)(ppdb_base_async_handle_t* handle, int status);

// IO operation types
typedef enum ppdb_base_async_op {
    PPDB_ASYNC_OP_NONE = 0,
    PPDB_ASYNC_OP_READ,
    PPDB_ASYNC_OP_WRITE
} ppdb_base_async_op_t;

// IO operation context
typedef struct ppdb_base_async_op_context {
    ppdb_base_async_op_t type;    // Operation type
    void* buf;                    // IO buffer
    size_t len;                   // Buffer length
    size_t pos;                   // Current position
    ppdb_base_async_cb callback;  // User callback
    void* user_data;             // User data
} ppdb_base_async_op_context_t;

// Common async handle structure
struct ppdb_base_async_handle {
    ppdb_base_async_loop_t* loop;  // Owner event loop
    int fd;                        // File descriptor
    void* impl_data;               // Implementation specific data
    ppdb_base_async_op_context_t op_ctx;  // Operation context
};

// Common async loop structure
struct ppdb_base_async_loop {
    bool is_running;              // Is event loop running
    ppdb_base_mutex_t* mutex;     // Protect internal state
    void* impl_data;              // Implementation specific data
};

// Implementation selection options
typedef enum ppdb_base_async_impl_type {
    PPDB_ASYNC_IMPL_AUTO = 0,  // Automatically select best implementation
    PPDB_ASYNC_IMPL_EPOLL,     // Force epoll implementation
    PPDB_ASYNC_IMPL_IOCP       // Force IOCP implementation
} ppdb_base_async_impl_type_t;

#endif // PPDB_BASE_ASYNC_COMMON_H_