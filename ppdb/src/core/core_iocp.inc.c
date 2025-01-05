//-----------------------------------------------------------------------------
// Windows IOCP Implementation
//-----------------------------------------------------------------------------

#ifdef _WIN32

struct ppdb_core_iocp_loop {
    HANDLE iocp;
    bool is_running;
    ppdb_core_mutex_t* mutex;
};

struct ppdb_core_iocp_handle {
    ppdb_core_iocp_loop_t* loop;
    HANDLE handle;
    void* data;
    ppdb_core_async_cb callback;
    OVERLAPPED overlapped;
    struct {
        void* buf;
        size_t len;
        size_t pos;
    } io;
};

ppdb_error_t ppdb_core_iocp_loop_create(ppdb_core_iocp_loop_t** loop) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    *loop = ppdb_core_alloc(sizeof(ppdb_core_iocp_loop_t));
    if (!*loop) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*loop, 0, sizeof(ppdb_core_iocp_loop_t));

    // 创建 IOCP
    (*loop)->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!(*loop)->iocp) {
        ppdb_core_free(*loop);
        *loop = NULL;
        return PPDB_ERR_INTERNAL;
    }

    // 创建互斥锁
    ppdb_error_t err = ppdb_core_mutex_create(&(*loop)->mutex);
    if (err != PPDB_OK) {
        CloseHandle((*loop)->iocp);
        ppdb_core_free(*loop);
        *loop = NULL;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_iocp_loop_destroy(ppdb_core_iocp_loop_t* loop) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    if (loop->mutex) {
        ppdb_core_mutex_destroy(loop->mutex);
    }

    if (loop->iocp) {
        CloseHandle(loop->iocp);
    }

    ppdb_core_free(loop);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_iocp_loop_run(ppdb_core_iocp_loop_t* loop, int timeout_ms) {
    if (!loop) return PPDB_ERR_NULL_POINTER;

    DWORD bytes;
    ULONG_PTR key;
    OVERLAPPED* overlapped;
    ppdb_core_iocp_handle_t* handle;

    ppdb_core_mutex_lock(loop->mutex);
    loop->is_running = true;
    ppdb_core_mutex_unlock(loop->mutex);

    while (1) {
        ppdb_core_mutex_lock(loop->mutex);
        if (!loop->is_running) {
            ppdb_core_mutex_unlock(loop->mutex);
            break;
        }
        ppdb_core_mutex_unlock(loop->mutex);

        BOOL success = GetQueuedCompletionStatus(loop->iocp,
                                               &bytes,
                                               &key,
                                               &overlapped,
                                               timeout_ms);

        if (!overlapped) {
            if (!success && GetLastError() != WAIT_TIMEOUT) {
                return PPDB_ERR_INTERNAL;
            }
            continue;
        }

        handle = CONTAINING_RECORD(overlapped, ppdb_core_iocp_handle_t, overlapped);
        if (!handle || !handle->callback) continue;

        int status = success ? (int)bytes : -1;
        handle->callback((ppdb_core_async_handle_t*)handle, status);
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_iocp_handle_create(ppdb_core_iocp_loop_t* loop,
                                         HANDLE win_handle,
                                         ppdb_core_iocp_handle_t** handle) {
    if (!loop || !handle) return PPDB_ERR_NULL_POINTER;
    if (win_handle == INVALID_HANDLE_VALUE) return PPDB_ERR_INVALID_ARGUMENT;

    *handle = ppdb_core_alloc(sizeof(ppdb_core_iocp_handle_t));
    if (!*handle) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*handle, 0, sizeof(ppdb_core_iocp_handle_t));
    (*handle)->loop = loop;
    (*handle)->handle = win_handle;

    // 关联到 IOCP
    if (!CreateIoCompletionPort(win_handle, loop->iocp, (ULONG_PTR)*handle, 0)) {
        ppdb_core_free(*handle);
        *handle = NULL;
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_iocp_handle_destroy(ppdb_core_iocp_handle_t* handle) {
    if (!handle) return PPDB_ERR_NULL_POINTER;

    if (handle->io.buf) {
        ppdb_core_free(handle->io.buf);
    }

    if (handle->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->handle);
    }

    ppdb_core_free(handle);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_iocp_read(ppdb_core_iocp_handle_t* handle,
                                void* buf,
                                size_t len,
                                ppdb_core_async_cb cb) {
    if (!handle || !buf || !cb) return PPDB_ERR_NULL_POINTER;
    if (len == 0) return PPDB_ERR_INVALID_ARGUMENT;

    handle->io.buf = buf;
    handle->io.len = len;
    handle->io.pos = 0;
    handle->callback = cb;

    memset(&handle->overlapped, 0, sizeof(OVERLAPPED));

    DWORD flags = 0;
    WSABUF wsabuf = {(ULONG)len, buf};
    
    if (WSARecv((SOCKET)handle->handle, &wsabuf, 1, NULL, &flags,
                &handle->overlapped, NULL) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            return PPDB_ERR_INTERNAL;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_iocp_write(ppdb_core_iocp_handle_t* handle,
                                 const void* buf,
                                 size_t len,
                                 ppdb_core_async_cb cb) {
    if (!handle || !buf || !cb) return PPDB_ERR_NULL_POINTER;
    if (len == 0) return PPDB_ERR_INVALID_ARGUMENT;

    handle->io.buf = (void*)buf;
    handle->io.len = len;
    handle->io.pos = 0;
    handle->callback = cb;

    memset(&handle->overlapped, 0, sizeof(OVERLAPPED));

    WSABUF wsabuf = {(ULONG)len, (char*)buf};
    
    if (WSASend((SOCKET)handle->handle, &wsabuf, 1, NULL, 0,
                &handle->overlapped, NULL) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING) {
            return PPDB_ERR_INTERNAL;
        }
    }

    return PPDB_OK;
}

#endif // _WIN32
