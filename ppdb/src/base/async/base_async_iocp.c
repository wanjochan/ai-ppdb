#include "base_async_impl.h"
#include "base_async_common.h"

#if defined(__COSMOPOLITAN__)

//-----------------------------------------------------------------------------
// Windows Types and Constants (without windows.h dependency)
//-----------------------------------------------------------------------------

typedef void* HANDLE;
typedef unsigned long DWORD;
typedef uint64_t ULONG_PTR;
typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        } DUMMYSTRUCTNAME;
        void* Pointer;
    } DUMMYUNIONNAME;
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

#define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
#define ERROR_IO_PENDING 997
#define WAIT_TIMEOUT 258
#define WINAPI __attribute__((__stdcall__))

//-----------------------------------------------------------------------------
// IOCP Function Types and Structures
//-----------------------------------------------------------------------------

// IOCP function types
typedef HANDLE (WINAPI *CreateIoCompletionPort_f)(
    HANDLE FileHandle,
    HANDLE ExistingCompletionPort,
    ULONG_PTR CompletionKey,
    DWORD NumberOfConcurrentThreads
);

typedef int (WINAPI *GetQueuedCompletionStatus_f)(
    HANDLE CompletionPort,
    DWORD* lpNumberOfBytesTransferred,
    ULONG_PTR* lpCompletionKey,
    LPOVERLAPPED* lpOverlapped,
    DWORD dwMilliseconds
);

typedef int (WINAPI *PostQueuedCompletionStatus_f)(
    HANDLE CompletionPort,
    DWORD dwNumberOfBytesTransferred,
    ULONG_PTR dwCompletionKey,
    LPOVERLAPPED lpOverlapped
);

typedef int (WINAPI *ReadFile_f)(
    HANDLE hFile,
    void* lpBuffer,
    DWORD nNumberOfBytesToRead,
    DWORD* lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
);

typedef int (WINAPI *WriteFile_f)(
    HANDLE hFile,
    const void* lpBuffer,
    DWORD nNumberOfBytesToWrite,
    DWORD* lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
);

typedef int (WINAPI *CloseHandle_f)(HANDLE hObject);
typedef DWORD (WINAPI *GetLastError_f)(void);

// IOCP-specific loop data
typedef struct iocp_loop_data {
    HANDLE iocp;  // IOCP handle
} iocp_loop_data_t;

// IOCP-specific handle data
typedef struct iocp_handle_data {
    HANDLE handle;     // Windows handle
    OVERLAPPED ovl;    // Overlapped structure
    ppdb_base_async_handle_t* parent;  // Parent handle
} iocp_handle_data_t;

// Implementation context
typedef struct iocp_context {
    bool initialized;
    void* kernel32;
    CreateIoCompletionPort_f CreateIoCompletionPort;
    GetQueuedCompletionStatus_f GetQueuedCompletionStatus;
    PostQueuedCompletionStatus_f PostQueuedCompletionStatus;
    ReadFile_f ReadFile;
    WriteFile_f WriteFile;
    CloseHandle_f CloseHandle;
    GetLastError_f GetLastError;
} iocp_context_t;

//-----------------------------------------------------------------------------
// Implementation Functions
//-----------------------------------------------------------------------------

static ppdb_error_t iocp_init(void** context) {
    iocp_context_t* ctx = ppdb_base_alloc(sizeof(iocp_context_t));
    if (!ctx) return PPDB_ERR_OUT_OF_MEMORY;

    memset(ctx, 0, sizeof(iocp_context_t));

    // Load kernel32.dll functions using Cosmopolitan's loader
    ctx->kernel32 = cosmo_dlopen("kernel32.dll", 0);
    if (!ctx->kernel32) {
        ppdb_base_free(ctx);
        return PPDB_ERR_INTERNAL;
    }

    // Load required functions
    ctx->CreateIoCompletionPort = (CreateIoCompletionPort_f)
        cosmo_dlsym(ctx->kernel32, "CreateIoCompletionPort");
    ctx->GetQueuedCompletionStatus = (GetQueuedCompletionStatus_f)
        cosmo_dlsym(ctx->kernel32, "GetQueuedCompletionStatus");
    ctx->PostQueuedCompletionStatus = (PostQueuedCompletionStatus_f)
        cosmo_dlsym(ctx->kernel32, "PostQueuedCompletionStatus");
    ctx->ReadFile = (ReadFile_f)
        cosmo_dlsym(ctx->kernel32, "ReadFile");
    ctx->WriteFile = (WriteFile_f)
        cosmo_dlsym(ctx->kernel32, "WriteFile");
    ctx->CloseHandle = (CloseHandle_f)
        cosmo_dlsym(ctx->kernel32, "CloseHandle");
    ctx->GetLastError = (GetLastError_f)
        cosmo_dlsym(ctx->kernel32, "GetLastError");

    // Verify all functions were loaded
    if (!ctx->CreateIoCompletionPort || !ctx->GetQueuedCompletionStatus ||
        !ctx->PostQueuedCompletionStatus || !ctx->ReadFile || !ctx->WriteFile ||
        !ctx->CloseHandle || !ctx->GetLastError) {
        cosmo_dlclose(ctx->kernel32);
        ppdb_base_free(ctx);
        return PPDB_ERR_INTERNAL;
    }

    ctx->initialized = true;
    *context = ctx;
    return PPDB_OK;
}

static void iocp_cleanup(void* context) {
    iocp_context_t* ctx = (iocp_context_t*)context;
    if (ctx) {
        if (ctx->kernel32) {
            cosmo_dlclose(ctx->kernel32);
        }
        ppdb_base_free(ctx);
    }
}

static ppdb_error_t iocp_create_loop(void* context, ppdb_base_async_loop_t** loop) {
    iocp_context_t* ctx = (iocp_context_t*)context;
    if (!ctx || !ctx->initialized) return PPDB_ERR_INVALID_STATE;

    *loop = ppdb_base_alloc(sizeof(ppdb_base_async_loop_t));
    if (!*loop) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*loop, 0, sizeof(ppdb_base_async_loop_t));

    // Create IOCP-specific data
    iocp_loop_data_t* loop_data = ppdb_base_alloc(sizeof(iocp_loop_data_t));
    if (!loop_data) {
        ppdb_base_free(*loop);
        *loop = NULL;
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Create IOCP
    loop_data->iocp = ctx->CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!loop_data->iocp) {
        ppdb_base_free(loop_data);
        ppdb_base_free(*loop);
        *loop = NULL;
        return PPDB_ERR_INTERNAL;
    }

    // Create mutex
    ppdb_error_t err = ppdb_base_mutex_create(&(*loop)->mutex);
    if (err != PPDB_OK) {
        CloseHandle(loop_data->iocp);
        ppdb_base_free(loop_data);
        ppdb_base_free(*loop);
        *loop = NULL;
        return err;
    }

    (*loop)->impl_data = loop_data;
    return PPDB_OK;
}

static ppdb_error_t iocp_destroy_loop(void* context, ppdb_base_async_loop_t* loop) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    iocp_loop_data_t* loop_data = (iocp_loop_data_t*)loop->impl_data;
    if (loop_data) {
        if (loop_data->iocp) {
            CloseHandle(loop_data->iocp);
        }
        ppdb_base_free(loop_data);
    }

    if (loop->mutex) {
        ppdb_base_mutex_destroy(loop->mutex);
    }

    ppdb_base_free(loop);
    return PPDB_OK;
}

static ppdb_error_t iocp_run_loop(void* context, ppdb_base_async_loop_t* loop, int timeout_ms) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    iocp_context_t* ctx = (iocp_context_t*)context;
    iocp_loop_data_t* loop_data = (iocp_loop_data_t*)loop->impl_data;
    if (!ctx || !loop_data) return PPDB_ERR_INVALID_STATE;

    ppdb_base_mutex_lock(loop->mutex);
    loop->is_running = true;
    ppdb_base_mutex_unlock(loop->mutex);

    while (loop->is_running) {
        DWORD bytes;
        ULONG_PTR key;
        OVERLAPPED* ovl;
        
        BOOL success = ctx->GetQueuedCompletionStatus(
            loop_data->iocp,
            &bytes,
            &key,
            &ovl,
            timeout_ms);

        if (!success && !ovl) {
            if (GetLastError() == WAIT_TIMEOUT) continue;
            return PPDB_ERR_INTERNAL;
        }

        ppdb_base_async_handle_t* handle = 
            CONTAINING_RECORD(ovl, iocp_handle_data_t, ovl)->handle;
        
        if (handle && handle->op_ctx.callback) {
            handle->op_ctx.callback(handle, success ? 0 : -1);
        }
    }

    return PPDB_OK;
}

static ppdb_error_t iocp_stop_loop(void* context, ppdb_base_async_loop_t* loop) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(loop->mutex);
    loop->is_running = false;
    ppdb_base_mutex_unlock(loop->mutex);

    return PPDB_OK;
}

static ppdb_error_t iocp_create_handle(void* context, ppdb_base_async_loop_t* loop,
                                     int fd, ppdb_base_async_handle_t** handle) {
    if (!loop || !handle) return PPDB_ERR_NULL_POINTER;

    iocp_context_t* ctx = (iocp_context_t*)context;
    iocp_loop_data_t* loop_data = (iocp_loop_data_t*)loop->impl_data;
    if (!ctx || !loop_data) return PPDB_ERR_INVALID_STATE;

    *handle = ppdb_base_alloc(sizeof(ppdb_base_async_handle_t));
    if (!*handle) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*handle, 0, sizeof(ppdb_base_async_handle_t));
    (*handle)->loop = loop;
    (*handle)->fd = fd;

    // Create IOCP-specific data
    iocp_handle_data_t* handle_data = ppdb_base_alloc(sizeof(iocp_handle_data_t));
    if (!handle_data) {
        ppdb_base_free(*handle);
        *handle = NULL;
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Convert fd to Windows HANDLE
    handle_data->handle = (HANDLE)_get_osfhandle(fd);
    if (handle_data->handle == INVALID_HANDLE_VALUE) {
        ppdb_base_free(handle_data);
        ppdb_base_free(*handle);
        *handle = NULL;
        return PPDB_ERR_INTERNAL;
    }

    // Associate with IOCP
    if (!ctx->CreateIoCompletionPort(handle_data->handle, loop_data->iocp,
                                   (ULONG_PTR)*handle, 0)) {
        ppdb_base_free(handle_data);
        ppdb_base_free(*handle);
        *handle = NULL;
        return PPDB_ERR_INTERNAL;
    }

    memset(&handle_data->ovl, 0, sizeof(OVERLAPPED));
    (*handle)->impl_data = handle_data;
    return PPDB_OK;
}

static ppdb_error_t iocp_destroy_handle(void* context, ppdb_base_async_handle_t* handle) {
    if (!handle) return PPDB_ERR_NULL_POINTER;

    iocp_handle_data_t* handle_data = (iocp_handle_data_t*)handle->impl_data;
    if (handle_data) {
        if (handle_data->handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_data->handle);
        }
        ppdb_base_free(handle_data);
    }

    ppdb_base_free(handle);
    return PPDB_OK;
}

static ppdb_error_t iocp_read(void* context, ppdb_base_async_handle_t* handle,
                             void* buf, size_t len, ppdb_base_async_cb cb) {
    if (!handle || !buf || !cb) return PPDB_ERR_NULL_POINTER;

    iocp_handle_data_t* handle_data = (iocp_handle_data_t*)handle->impl_data;
    if (!handle_data) return PPDB_ERR_INVALID_STATE;

    handle->op_ctx.type = PPDB_ASYNC_OP_READ;
    handle->op_ctx.buf = buf;
    handle->op_ctx.len = len;
    handle->op_ctx.pos = 0;
    handle->op_ctx.callback = cb;

    memset(&handle_data->ovl, 0, sizeof(OVERLAPPED));
    handle_data->ovl.Offset = handle->op_ctx.pos;

    BOOL success = ReadFile(
        handle_data->handle,
        handle->op_ctx.buf,
        (DWORD)handle->op_ctx.len,
        NULL,
        &handle_data->ovl
    );

    if (!success && GetLastError() != ERROR_IO_PENDING) {
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
}

static ppdb_error_t iocp_write(void* context, ppdb_base_async_handle_t* handle,
                              const void* buf, size_t len, ppdb_base_async_cb cb) {
    if (!handle || !buf || !cb) return PPDB_ERR_NULL_POINTER;

    iocp_handle_data_t* handle_data = (iocp_handle_data_t*)handle->impl_data;
    if (!handle_data) return PPDB_ERR_INVALID_STATE;

    handle->op_ctx.type = PPDB_ASYNC_OP_WRITE;
    handle->op_ctx.buf = (void*)buf;
    handle->op_ctx.len = len;
    handle->op_ctx.pos = 0;
    handle->op_ctx.callback = cb;

    memset(&handle_data->ovl, 0, sizeof(OVERLAPPED));
    handle_data->ovl.Offset = handle->op_ctx.pos;

    BOOL success = WriteFile(
        handle_data->handle,
        handle->op_ctx.buf,
        (DWORD)handle->op_ctx.len,
        NULL,
        &handle_data->ovl
    );

    if (!success && GetLastError() != ERROR_IO_PENDING) {
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Implementation Instance
//-----------------------------------------------------------------------------

static ppdb_base_async_impl_t iocp_impl = {
    .name = "iocp",
    .init = iocp_init,
    .cleanup = iocp_cleanup,
    .create_loop = iocp_create_loop,
    .destroy_loop = iocp_destroy_loop,
    .run_loop = iocp_run_loop,
    .stop_loop = iocp_stop_loop,
    .create_handle = iocp_create_handle,
    .destroy_handle = iocp_destroy_handle,
    .read = iocp_read,
    .write = iocp_write,
    .impl_data = NULL
};

const ppdb_base_async_impl_t* ppdb_base_async_get_iocp_impl(void) {
    return &iocp_impl;
}

#endif // __COSMOPOLITAN__