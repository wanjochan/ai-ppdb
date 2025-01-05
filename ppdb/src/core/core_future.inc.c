//-----------------------------------------------------------------------------
// Future Pattern Implementation
//-----------------------------------------------------------------------------

struct ppdb_core_future {
    ppdb_core_async_loop_t* loop;
    bool is_ready;
    void* result;
    size_t result_size;
    ppdb_core_mutex_t* mutex;
    ppdb_core_cond_t* cond;
    ppdb_core_async_cb on_complete;
    void* user_data;
    ppdb_error_t error;
};

ppdb_error_t ppdb_core_future_create(ppdb_core_async_loop_t* loop,
                                    ppdb_core_future_t** future) {
    if (!loop || !future) return PPDB_ERR_NULL_POINTER;

    *future = ppdb_core_alloc(sizeof(ppdb_core_future_t));
    if (!*future) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*future, 0, sizeof(ppdb_core_future_t));
    (*future)->loop = loop;

    // 创建互斥锁
    ppdb_error_t err = ppdb_core_mutex_create(&(*future)->mutex);
    if (err != PPDB_OK) {
        ppdb_core_free(*future);
        *future = NULL;
        return err;
    }

    // 创建条件变量
    err = ppdb_core_cond_create(&(*future)->cond);
    if (err != PPDB_OK) {
        ppdb_core_mutex_destroy((*future)->mutex);
        ppdb_core_free(*future);
        *future = NULL;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_future_destroy(ppdb_core_future_t* future) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    if (future->mutex) {
        ppdb_core_mutex_destroy(future->mutex);
    }

    if (future->cond) {
        ppdb_core_cond_destroy(future->cond);
    }

    if (future->result) {
        ppdb_core_free(future->result);
    }

    ppdb_core_free(future);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_future_set_callback(ppdb_core_future_t* future,
                                          ppdb_core_async_cb cb,
                                          void* user_data) {
    if (!future || !cb) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(future->mutex);
    future->on_complete = cb;
    future->user_data = user_data;
    ppdb_core_mutex_unlock(future->mutex);

    return PPDB_OK;
}

ppdb_error_t ppdb_core_future_set_result(ppdb_core_future_t* future,
                                        void* result,
                                        size_t size) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(future->mutex);

    if (future->result) {
        ppdb_core_free(future->result);
    }

    if (result && size > 0) {
        future->result = ppdb_core_alloc(size);
        if (!future->result) {
            ppdb_core_mutex_unlock(future->mutex);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memcpy(future->result, result, size);
        future->result_size = size;
    }

    future->is_ready = true;
    future->error = PPDB_OK;

    // 通知等待者
    ppdb_core_cond_broadcast(future->cond);

    // 如果有回调，执行它
    if (future->on_complete) {
        future->on_complete((ppdb_core_async_handle_t*)future, 0);
    }

    ppdb_core_mutex_unlock(future->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_future_set_error(ppdb_core_future_t* future,
                                       ppdb_error_t error) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(future->mutex);

    future->is_ready = true;
    future->error = error;

    // 通知等待者
    ppdb_core_cond_broadcast(future->cond);

    // 如果有回调，执行它
    if (future->on_complete) {
        future->on_complete((ppdb_core_async_handle_t*)future, -1);
    }

    ppdb_core_mutex_unlock(future->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_future_wait(ppdb_core_future_t* future) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(future->mutex);
    while (!future->is_ready) {
        ppdb_core_cond_wait(future->cond, future->mutex);
    }
    ppdb_error_t err = future->error;
    ppdb_core_mutex_unlock(future->mutex);

    return err;
}

ppdb_error_t ppdb_core_future_wait_timeout(ppdb_core_future_t* future,
                                          uint32_t timeout_ms) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(future->mutex);
    
    if (!future->is_ready) {
        ppdb_error_t err = ppdb_core_cond_timedwait(future->cond, future->mutex, timeout_ms);
        if (err != PPDB_OK) {
            ppdb_core_mutex_unlock(future->mutex);
            return err;
        }
    }

    ppdb_error_t err = future->error;
    ppdb_core_mutex_unlock(future->mutex);

    return err;
}

ppdb_error_t ppdb_core_future_is_ready(ppdb_core_future_t* future,
                                      bool* ready) {
    if (!future || !ready) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(future->mutex);
    *ready = future->is_ready;
    ppdb_core_mutex_unlock(future->mutex);

    return PPDB_OK;
}

ppdb_error_t ppdb_core_future_get_result(ppdb_core_future_t* future,
                                        void* result,
                                        size_t size,
                                        size_t* actual_size) {
    if (!future || !result || !actual_size) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_lock(future->mutex);

    if (!future->is_ready) {
        ppdb_core_mutex_unlock(future->mutex);
        return PPDB_ERR_NOT_READY;
    }

    if (future->error != PPDB_OK) {
        ppdb_core_mutex_unlock(future->mutex);
        return future->error;
    }

    if (!future->result || future->result_size == 0) {
        *actual_size = 0;
        ppdb_core_mutex_unlock(future->mutex);
        return PPDB_OK;
    }

    *actual_size = (size < future->result_size) ? size : future->result_size;
    memcpy(result, future->result, *actual_size);

    ppdb_core_mutex_unlock(future->mutex);
    return PPDB_OK;
}
