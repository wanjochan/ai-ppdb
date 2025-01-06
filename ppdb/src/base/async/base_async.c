#include "base_async_impl.h"
#include "base_async_common.h"

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

static const ppdb_base_async_impl_t* current_impl = NULL;
static void* impl_context = NULL;

//-----------------------------------------------------------------------------
// Implementation Selection
//-----------------------------------------------------------------------------

const ppdb_base_async_impl_t* ppdb_base_async_get_default_impl(void) {
#if defined(__COSMOPOLITAN__)
    if (IsWindows()) {
        return ppdb_base_async_get_iocp_impl();
    }
#endif
    return ppdb_base_async_get_epoll_impl();
}

//-----------------------------------------------------------------------------
// Initialization and Cleanup
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_async_init(ppdb_base_async_impl_type_t impl_type) {
    if (current_impl) {
        return PPDB_ERR_ALREADY_INITIALIZED;
    }

    // Select implementation
    switch (impl_type) {
        case PPDB_ASYNC_IMPL_EPOLL:
            current_impl = ppdb_base_async_get_epoll_impl();
            break;
        case PPDB_ASYNC_IMPL_IOCP:
#if defined(__COSMOPOLITAN__)
            current_impl = ppdb_base_async_get_iocp_impl();
#else
            return PPDB_ERR_NOT_SUPPORTED;
#endif
            break;
        case PPDB_ASYNC_IMPL_AUTO:
        default:
            current_impl = ppdb_base_async_get_default_impl();
            break;
    }

    if (!current_impl) {
        return PPDB_ERR_INTERNAL;
    }

    // Initialize implementation
    return current_impl->init(&impl_context);
}

void ppdb_base_async_cleanup(void) {
    if (current_impl && impl_context) {
        current_impl->cleanup(impl_context);
        current_impl = NULL;
        impl_context = NULL;
    }
}

//-----------------------------------------------------------------------------
// Event Loop Operations
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_async_loop_create(ppdb_base_async_loop_t** loop) {
    if (!current_impl || !impl_context) {
        return PPDB_ERR_NOT_INITIALIZED;
    }
    return current_impl->create_loop(impl_context, loop);
}

ppdb_error_t ppdb_base_async_loop_destroy(ppdb_base_async_loop_t* loop) {
    if (!current_impl || !impl_context) {
        return PPDB_ERR_NOT_INITIALIZED;
    }
    return current_impl->destroy_loop(impl_context, loop);
}

ppdb_error_t ppdb_base_async_loop_run(ppdb_base_async_loop_t* loop, int timeout_ms) {
    if (!current_impl || !impl_context) {
        return PPDB_ERR_NOT_INITIALIZED;
    }
    return current_impl->run_loop(impl_context, loop, timeout_ms);
}

ppdb_error_t ppdb_base_async_loop_stop(ppdb_base_async_loop_t* loop) {
    if (!current_impl || !impl_context) {
        return PPDB_ERR_NOT_INITIALIZED;
    }
    return current_impl->stop_loop(impl_context, loop);
}

//-----------------------------------------------------------------------------
// Handle Operations
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_async_handle_create(ppdb_base_async_loop_t* loop,
                                          int fd,
                                          ppdb_base_async_handle_t** handle) {
    if (!current_impl || !impl_context) {
        return PPDB_ERR_NOT_INITIALIZED;
    }
    return current_impl->create_handle(impl_context, loop, fd, handle);
}

ppdb_error_t ppdb_base_async_handle_destroy(ppdb_base_async_handle_t* handle) {
    if (!current_impl || !impl_context) {
        return PPDB_ERR_NOT_INITIALIZED;
    }
    return current_impl->destroy_handle(impl_context, handle);
}

ppdb_error_t ppdb_base_async_read(ppdb_base_async_handle_t* handle,
                                 void* buf,
                                 size_t len,
                                 ppdb_base_async_cb cb) {
    if (!current_impl || !impl_context) {
        return PPDB_ERR_NOT_INITIALIZED;
    }
    return current_impl->read(impl_context, handle, buf, len, cb);
}

ppdb_error_t ppdb_base_async_write(ppdb_base_async_handle_t* handle,
                                  const void* buf,
                                  size_t len,
                                  ppdb_base_async_cb cb) {
    if (!current_impl || !impl_context) {
        return PPDB_ERR_NOT_INITIALIZED;
    }
    return current_impl->write(impl_context, handle, buf, len, cb);
}

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

const char* ppdb_base_async_get_impl_name(void) {
    if (!current_impl) {
        return "none";
    }
    return current_impl->name;
}

bool ppdb_base_async_is_initialized(void) {
    return current_impl != NULL && impl_context != NULL;
}