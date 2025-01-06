#include "base_async_impl.h"
#include "base_async_common.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

//-----------------------------------------------------------------------------
// Epoll Implementation
//-----------------------------------------------------------------------------

// Epoll-specific loop data
typedef struct epoll_loop_data {
    int epoll_fd;  // epoll file descriptor
} epoll_loop_data_t;

// Epoll-specific handle data
typedef struct epoll_handle_data {
    struct epoll_event event;  // epoll event structure
} epoll_handle_data_t;

// Implementation context
typedef struct epoll_context {
    bool initialized;
} epoll_context_t;

//-----------------------------------------------------------------------------
// Implementation Functions
//-----------------------------------------------------------------------------

static ppdb_error_t epoll_init(void** context) {
    epoll_context_t* ctx = ppdb_base_alloc(sizeof(epoll_context_t));
    if (!ctx) return PPDB_ERR_OUT_OF_MEMORY;

    memset(ctx, 0, sizeof(epoll_context_t));
    ctx->initialized = true;
    *context = ctx;
    
    return PPDB_OK;
}

static void epoll_cleanup(void* context) {
    epoll_context_t* ctx = (epoll_context_t*)context;
    if (ctx) {
        ppdb_base_free(ctx);
    }
}

static ppdb_error_t epoll_create_loop(void* context, ppdb_base_async_loop_t** loop) {
    epoll_context_t* ctx = (epoll_context_t*)context;
    if (!ctx || !ctx->initialized) return PPDB_ERR_INVALID_STATE;

    *loop = ppdb_base_alloc(sizeof(ppdb_base_async_loop_t));
    if (!*loop) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*loop, 0, sizeof(ppdb_base_async_loop_t));

    // Create epoll-specific data
    epoll_loop_data_t* loop_data = ppdb_base_alloc(sizeof(epoll_loop_data_t));
    if (!loop_data) {
        ppdb_base_free(*loop);
        *loop = NULL;
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Create epoll instance
    loop_data->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop_data->epoll_fd < 0) {
        ppdb_base_free(loop_data);
        ppdb_base_free(*loop);
        *loop = NULL;
        return PPDB_ERR_INTERNAL;
    }

    // Create mutex
    ppdb_error_t err = ppdb_base_mutex_create(&(*loop)->mutex);
    if (err != PPDB_OK) {
        close(loop_data->epoll_fd);
        ppdb_base_free(loop_data);
        ppdb_base_free(*loop);
        *loop = NULL;
        return err;
    }

    (*loop)->impl_data = loop_data;
    return PPDB_OK;
}

static ppdb_error_t epoll_destroy_loop(void* context, ppdb_base_async_loop_t* loop) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    epoll_loop_data_t* loop_data = (epoll_loop_data_t*)loop->impl_data;
    if (loop_data) {
        if (loop_data->epoll_fd >= 0) {
            close(loop_data->epoll_fd);
        }
        ppdb_base_free(loop_data);
    }

    if (loop->mutex) {
        ppdb_base_mutex_destroy(loop->mutex);
    }

    ppdb_base_free(loop);
    return PPDB_OK;
}

static ppdb_error_t epoll_run_loop(void* context, ppdb_base_async_loop_t* loop, int timeout_ms) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    epoll_loop_data_t* loop_data = (epoll_loop_data_t*)loop->impl_data;
    if (!loop_data) return PPDB_ERR_INVALID_STATE;

    ppdb_base_mutex_lock(loop->mutex);
    loop->is_running = true;
    ppdb_base_mutex_unlock(loop->mutex);

    struct epoll_event events[64];
    int nfds;

    while (loop->is_running) {
        nfds = epoll_wait(loop_data->epoll_fd, events, 64, timeout_ms);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            return PPDB_ERR_INTERNAL;
        }

        for (int i = 0; i < nfds; i++) {
            ppdb_base_async_handle_t* handle = events[i].data.ptr;
            if (handle && handle->op_ctx.callback) {
                handle->op_ctx.callback(handle, 0);
            }
        }
    }

    return PPDB_OK;
}

static ppdb_error_t epoll_stop_loop(void* context, ppdb_base_async_loop_t* loop) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(loop->mutex);
    loop->is_running = false;
    ppdb_base_mutex_unlock(loop->mutex);

    return PPDB_OK;
}

static ppdb_error_t epoll_create_handle(void* context, ppdb_base_async_loop_t* loop,
                                      int fd, ppdb_base_async_handle_t** handle) {
    if (!loop || !handle) return PPDB_ERR_NULL_POINTER;

    epoll_loop_data_t* loop_data = (epoll_loop_data_t*)loop->impl_data;
    if (!loop_data) return PPDB_ERR_INVALID_STATE;

    *handle = ppdb_base_alloc(sizeof(ppdb_base_async_handle_t));
    if (!*handle) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*handle, 0, sizeof(ppdb_base_async_handle_t));
    (*handle)->loop = loop;
    (*handle)->fd = fd;

    // Create epoll-specific data
    epoll_handle_data_t* handle_data = ppdb_base_alloc(sizeof(epoll_handle_data_t));
    if (!handle_data) {
        ppdb_base_free(*handle);
        *handle = NULL;
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Setup epoll event
    handle_data->event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    handle_data->event.data.ptr = *handle;

    if (epoll_ctl(loop_data->epoll_fd, EPOLL_CTL_ADD, fd, &handle_data->event) < 0) {
        ppdb_base_free(handle_data);
        ppdb_base_free(*handle);
        *handle = NULL;
        return PPDB_ERR_INTERNAL;
    }

    (*handle)->impl_data = handle_data;
    return PPDB_OK;
}

static ppdb_error_t epoll_destroy_handle(void* context, ppdb_base_async_handle_t* handle) {
    if (!handle) return PPDB_ERR_NULL_POINTER;

    epoll_loop_data_t* loop_data = (epoll_loop_data_t*)handle->loop->impl_data;
    if (loop_data && handle->fd >= 0) {
        epoll_ctl(loop_data->epoll_fd, EPOLL_CTL_DEL, handle->fd, NULL);
    }

    if (handle->impl_data) {
        ppdb_base_free(handle->impl_data);
    }

    ppdb_base_free(handle);
    return PPDB_OK;
}

static ppdb_error_t epoll_read(void* context, ppdb_base_async_handle_t* handle,
                              void* buf, size_t len, ppdb_base_async_cb cb) {
    if (!handle || !buf || !cb) return PPDB_ERR_NULL_POINTER;

    handle->op_ctx.type = PPDB_ASYNC_OP_READ;
    handle->op_ctx.buf = buf;
    handle->op_ctx.len = len;
    handle->op_ctx.pos = 0;
    handle->op_ctx.callback = cb;

    return PPDB_OK;
}

static ppdb_error_t epoll_write(void* context, ppdb_base_async_handle_t* handle,
                               const void* buf, size_t len, ppdb_base_async_cb cb) {
    if (!handle || !buf || !cb) return PPDB_ERR_NULL_POINTER;

    handle->op_ctx.type = PPDB_ASYNC_OP_WRITE;
    handle->op_ctx.buf = (void*)buf;
    handle->op_ctx.len = len;
    handle->op_ctx.pos = 0;
    handle->op_ctx.callback = cb;

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Implementation Instance
//-----------------------------------------------------------------------------

static ppdb_base_async_impl_t epoll_impl = {
    .name = "epoll",
    .init = epoll_init,
    .cleanup = epoll_cleanup,
    .create_loop = epoll_create_loop,
    .destroy_loop = epoll_destroy_loop,
    .run_loop = epoll_run_loop,
    .stop_loop = epoll_stop_loop,
    .create_handle = epoll_create_handle,
    .destroy_handle = epoll_destroy_handle,
    .read = epoll_read,
    .write = epoll_write,
    .impl_data = NULL
};

const ppdb_base_async_impl_t* ppdb_base_async_get_epoll_impl(void) {
    return &epoll_impl;
}