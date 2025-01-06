#include <cosmopolitan.h>
#include "../internal/base.h"

// Internal async context structure
struct ppdb_base_async_ctx {
    int io_handle;  // unified I/O handle for async operations
    bool is_running;
    ppdb_base_error_t last_error;
};

// Initialize async context
ppdb_base_error_t ppdb_base_async_init(ppdb_base_async_ctx_t* ctx) {
    if (!ctx) {
        return PPDB_BASE_ERROR_INVALID_PARAM;
    }

    memset(ctx, 0, sizeof(ppdb_base_async_ctx_t));
    
    ctx->io_handle = kCreateIoHandle();
    if (ctx->io_handle < 0) {
        return PPDB_BASE_ERROR_SYSTEM;
    }

    ctx->is_running = true;
    return PPDB_BASE_ERROR_OK;
}

// Run event loop
ppdb_base_error_t ppdb_base_async_run(ppdb_base_async_ctx_t* ctx) {
    if (!ctx) {
        return PPDB_BASE_ERROR_INVALID_PARAM;
    }

    struct IoEvent events[32];
    
    while (ctx->is_running) {
        int nevents = WaitForIoEvents(ctx->io_handle, events, 32, -1);
        if (nevents < 0) {
            if (IsInterrupted()) continue;
            ctx->last_error = PPDB_BASE_ERROR_SYSTEM;
            return PPDB_BASE_ERROR_SYSTEM;
        }

        for (int i = 0; i < nevents; i++) {
            ppdb_base_event_handler_t* handler = 
                (ppdb_base_event_handler_t*)events[i].data;
            handler->callback(handler->data);
        }
    }

    return PPDB_BASE_ERROR_OK;
}

// Stop event loop
void ppdb_base_async_stop(ppdb_base_async_ctx_t* ctx) {
    if (ctx) {
        ctx->is_running = false;
    }
}

// Cleanup async context
void ppdb_base_async_cleanup(ppdb_base_async_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->io_handle >= 0) {
        CloseIoHandle(ctx->io_handle);
        ctx->io_handle = -1;
    }
}

// ... additional async utility functions ...

//-----------------------------------------------------------------------------
// Platform-specific definitions and functions
//-----------------------------------------------------------------------------

#if defined(__COSMOPOLITAN__)

// Windows IOCP function types
typedef HANDLE (WINAPI *CreateIoCompletionPort_t)(
    HANDLE FileHandle,
    HANDLE ExistingCompletionPort,
    ULONG_PTR CompletionKey,
    DWORD NumberOfConcurrentThreads
);

typedef BOOL (WINAPI *GetQueuedCompletionStatus_t)(
    HANDLE CompletionPort,
    LPDWORD lpNumberOfBytesTransferred,
    PULONG_PTR lpCompletionKey,
    LPOVERLAPPED* lpOverlapped,
    DWORD dwMilliseconds
);

typedef BOOL (WINAPI *PostQueuedCompletionStatus_t)(
    HANDLE CompletionPort,
    DWORD dwNumberOfBytesTransferred,
    ULONG_PTR dwCompletionKey,
    LPOVERLAPPED lpOverlapped
);

// Dynamic function pointers
static struct {
    CreateIoCompletionPort_t CreateIoCompletionPort;
    GetQueuedCompletionStatus_t GetQueuedCompletionStatus;
    PostQueuedCompletionStatus_t PostQueuedCompletionStatus;
} win32;

// Initialize Windows IOCP functions
static ppdb_error_t init_win32_iocp(void) {
    void* kernel32 = dlopen("kernel32.dll", 0);
    if (!kernel32) return PPDB_ERR_INTERNAL;

    win32.CreateIoCompletionPort = (CreateIoCompletionPort_t)
        dlsym(kernel32, "CreateIoCompletionPort");
    win32.GetQueuedCompletionStatus = (GetQueuedCompletionStatus_t)
        dlsym(kernel32, "GetQueuedCompletionStatus");
    win32.PostQueuedCompletionStatus = (PostQueuedCompletionStatus_t)
        dlsym(kernel32, "PostQueuedCompletionStatus");

    if (!win32.CreateIoCompletionPort ||
        !win32.GetQueuedCompletionStatus ||
        !win32.PostQueuedCompletionStatus) {
        dlclose(kernel32);
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
}

#endif

//-----------------------------------------------------------------------------
// Asynchronous Operations Implementation
//-----------------------------------------------------------------------------

struct ppdb_base_async_loop {
#if defined(__COSMOPOLITAN__)
    HANDLE iocp;                 // Windows IOCP handle
#else
    int epoll_fd;               // Linux epoll file descriptor
#endif
    bool is_running;            // is event loop running
    ppdb_base_mutex_t* mutex;   // protect internal state
};

struct ppdb_base_async_handle {
    ppdb_base_async_loop_t* loop;  // owner event loop
#if defined(__COSMOPOLITAN__)
    HANDLE handle;                 // Windows handle
    OVERLAPPED overlapped;         // Windows overlapped structure
#else
    int fd;                        // Unix file descriptor
#endif
    void* data;                    // user data
    ppdb_base_async_cb callback;   // callback function
    struct {
        void* buf;                 // IO buffer
        size_t len;                // buffer length
        size_t pos;                // current position
    } io;
};

ppdb_error_t ppdb_base_async_loop_create(ppdb_base_async_loop_t** loop) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    *loop = ppdb_base_alloc(sizeof(ppdb_base_async_loop_t));
    if (!*loop) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*loop, 0, sizeof(ppdb_base_async_loop_t));

#if defined(__COSMOPOLITAN__)
    // Initialize Windows IOCP
    ppdb_error_t err = init_win32_iocp();
    if (err != PPDB_OK) {
        ppdb_base_free(*loop);
        *loop = NULL;
        return err;
    }

    (*loop)->iocp = win32.CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, NULL, 0, 0);
    if ((*loop)->iocp == NULL) {
        ppdb_base_free(*loop);
        *loop = NULL;
        return PPDB_ERR_INTERNAL;
    }
#else
    // Initialize Linux epoll
    (*loop)->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if ((*loop)->epoll_fd < 0) {
        ppdb_base_free(*loop);
        *loop = NULL;
        return PPDB_ERR_INTERNAL;
    }
#endif

    ppdb_error_t err = ppdb_base_mutex_create(&(*loop)->mutex);
    if (err != PPDB_OK) {
#if defined(__COSMOPOLITAN__)
        CloseHandle((*loop)->iocp);
#else
        close((*loop)->epoll_fd);
#endif
        ppdb_base_free(*loop);
        *loop = NULL;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_async_loop_destroy(ppdb_base_async_loop_t* loop) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    if (loop->mutex) {
        ppdb_base_mutex_destroy(loop->mutex);
    }

#if defined(__COSMOPOLITAN__)
    if (loop->iocp) {
        CloseHandle(loop->iocp);
    }
#else
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
#endif

    ppdb_base_free(loop);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_async_loop_run(ppdb_base_async_loop_t* loop, int timeout_ms) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(loop->mutex);
    loop->is_running = true;
    ppdb_base_mutex_unlock(loop->mutex);

#if defined(__COSMOPOLITAN__)
    // Windows IOCP event loop
    while (loop->is_running) {
        DWORD bytes_transferred;
        ULONG_PTR completion_key;
        OVERLAPPED* overlapped;
        
        BOOL success = win32.GetQueuedCompletionStatus(
            loop->iocp,
            &bytes_transferred,
            &completion_key,
            &overlapped,
            timeout_ms);

        if (!success && !overlapped) {
            if (GetLastError() == WAIT_TIMEOUT) continue;
            return PPDB_ERR_INTERNAL;
        }

        ppdb_base_async_handle_t* handle = 
            CONTAINING_RECORD(overlapped, ppdb_base_async_handle_t, overlapped);
        
        if (handle && handle->callback) {
            handle->callback(handle, success ? 0 : -1);
        }
    }
#else
    // Linux epoll event loop
    struct epoll_event events[64];
    int nfds;

    while (loop->is_running) {
        nfds = epoll_wait(loop->epoll_fd, events, 64, timeout_ms);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            return PPDB_ERR_INTERNAL;
        }

        for (int i = 0; i < nfds; i++) {
            ppdb_base_async_handle_t* handle = events[i].data.ptr;
            if (handle && handle->callback) {
                handle->callback(handle, 0);
            }
        }
    }
#endif

    return PPDB_OK;
}

ppdb_error_t ppdb_base_async_handle_create(ppdb_base_async_loop_t* loop,
                                          int fd,
                                          ppdb_base_async_handle_t** handle) {
    if (!loop || !handle) return PPDB_ERR_NULL_POINTER;

    *handle = ppdb_base_alloc(sizeof(ppdb_base_async_handle_t));
    if (!*handle) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*handle, 0, sizeof(ppdb_base_async_handle_t));
    (*handle)->loop = loop;

#if defined(__COSMOPOLITAN__)
    // Convert fd to Windows HANDLE
    (*handle)->handle = (HANDLE)_get_osfhandle(fd);
    if ((*handle)->handle == INVALID_HANDLE_VALUE) {
        ppdb_base_free(*handle);
        *handle = NULL;
        return PPDB_ERR_INTERNAL;
    }

    // Associate with IOCP
    if (win32.CreateIoCompletionPort((*handle)->handle, loop->iocp,
                                   (ULONG_PTR)*handle, 0) == NULL) {
        ppdb_base_free(*handle);
        *handle = NULL;
        return PPDB_ERR_INTERNAL;
    }
#else
    (*handle)->fd = fd;

    struct epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.ptr = *handle;

    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        ppdb_base_free(*handle);
        *handle = NULL;
        return PPDB_ERR_INTERNAL;
    }
#endif

    return PPDB_OK;
}

ppdb_error_t ppdb_base_async_handle_destroy(ppdb_base_async_handle_t* handle) {
    if (!handle) return PPDB_ERR_NULL_POINTER;

#if defined(__COSMOPOLITAN__)
    if (handle->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->handle);
    }
#else
    if (handle->fd >= 0) {
        epoll_ctl(handle->loop->epoll_fd, EPOLL_CTL_DEL, handle->fd, NULL);
    }
#endif

    ppdb_base_free(handle);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_async_read(ppdb_base_async_handle_t* handle,
                                 void* buf,
                                 size_t len,
                                 ppdb_base_async_cb cb) {
    if (!handle || !buf || !cb) return PPDB_ERR_NULL_POINTER;

    handle->io.buf = buf;
    handle->io.len = len;
    handle->io.pos = 0;
    handle->callback = cb;

#if defined(__COSMOPOLITAN__)
    memset(&handle->overlapped, 0, sizeof(OVERLAPPED));
    handle->overlapped.Offset = handle->io.pos;

    BOOL success = ReadFile(
        handle->handle,
        handle->io.buf,
        (DWORD)handle->io.len,
        NULL,
        &handle->overlapped
    );

    if (!success && GetLastError() != ERROR_IO_PENDING) {
        return PPDB_ERR_INTERNAL;
    }
#endif

    return PPDB_OK;
}

ppdb_error_t ppdb_base_async_write(ppdb_base_async_handle_t* handle,
                                  const void* buf,
                                  size_t len,
                                  ppdb_base_async_cb cb) {
    if (!handle || !buf || !cb) return PPDB_ERR_NULL_POINTER;

    handle->io.buf = (void*)buf;
    handle->io.len = len;
    handle->io.pos = 0;
    handle->callback = cb;

#if defined(__COSMOPOLITAN__)
    memset(&handle->overlapped, 0, sizeof(OVERLAPPED));
    handle->overlapped.Offset = handle->io.pos;

    BOOL success = WriteFile(
        handle->handle,
        handle->io.buf,
        (DWORD)handle->io.len,
        NULL,
        &handle->overlapped
    );

    if (!success && GetLastError() != ERROR_IO_PENDING) {
        return PPDB_ERR_INTERNAL;
    }
#endif

    return PPDB_OK;
}

ppdb_error_t ppdb_base_async_create(ppdb_base_async_loop_t** loop) {
    const char* impl_name = getenv("PPDB_ASYNC_IMPL");
    const ppdb_base_async_impl_t* impl = NULL;

    if (impl_name) {
        // User explicitly specified implementation
        if (strcmp(impl_name, "iocp") == 0) {
            impl = ppdb_base_async_get_iocp_impl();
        } else if (strcmp(impl_name, "epoll") == 0) {
            impl = ppdb_base_async_get_epoll_impl();
        }
    } else {
        // Auto-select implementation based on OS
        if (ppdb_base_is_windows()) {
            impl = ppdb_base_async_get_iocp_impl();
        } else if (ppdb_base_is_unix()) {
            impl = ppdb_base_async_get_epoll_impl();
        }
    }

    if (!impl) {
        return PPDB_ERR_NOT_SUPPORTED;
    }

    return ppdb_base_async_create_with_impl(impl, loop);
}