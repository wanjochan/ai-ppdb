#include "base.h"
#include "base_internal.h"

// Future state definitions
typedef enum {
    PPDB_FUTURE_PENDING,
    PPDB_FUTURE_COMPLETED,
    PPDB_FUTURE_FAILED
} ppdb_future_state;

struct ppdb_future {
    ppdb_future_state state;
    void* result;
    char* error;
    ppdb_mutex_t mutex;
    ppdb_cond_t cond;
    ppdb_future_callback callback;
    void* callback_arg;
};

ppdb_future_t* ppdb_base_future_new(void) {
    ppdb_future_t* future = (ppdb_future_t*)malloc(sizeof(ppdb_future_t));
    if (!future) {
        return NULL;
    }
    
    future->state = PPDB_FUTURE_PENDING;
    future->result = NULL;
    future->error = NULL;
    future->callback = NULL;
    future->callback_arg = NULL;
    
    ppdb_mutex_init(&future->mutex);
    ppdb_cond_init(&future->cond);
    
    return future;
}

void ppdb_base_future_free(ppdb_future_t* future) {
    if (future) {
        if (future->error) {
            free(future->error);
        }
        ppdb_mutex_destroy(&future->mutex);
        ppdb_cond_destroy(&future->cond);
        free(future);
    }
}

void ppdb_base_future_set_callback(ppdb_future_t* future, 
                                 ppdb_future_callback callback, 
                                 void* arg) {
    ppdb_mutex_lock(&future->mutex);
    future->callback = callback;
    future->callback_arg = arg;
    ppdb_mutex_unlock(&future->mutex);
}

void ppdb_base_future_complete(ppdb_future_t* future, void* result) {
    ppdb_mutex_lock(&future->mutex);
    future->state = PPDB_FUTURE_COMPLETED;
    future->result = result;
    
    if (future->callback) {
        future->callback(future, future->callback_arg);
    }
    
    ppdb_cond_broadcast(&future->cond);
    ppdb_mutex_unlock(&future->mutex);
}

void ppdb_base_future_fail(ppdb_future_t* future, const char* error_msg) {
    ppdb_mutex_lock(&future->mutex);
    future->state = PPDB_FUTURE_FAILED;
    if (error_msg) {
        future->error = strdup(error_msg);
    }
    
    if (future->callback) {
        future->callback(future, future->callback_arg);
    }
    
    ppdb_cond_broadcast(&future->cond);
    ppdb_mutex_unlock(&future->mutex);
}

void* ppdb_base_future_get(ppdb_future_t* future) {
    ppdb_mutex_lock(&future->mutex);
    while (future->state == PPDB_FUTURE_PENDING) {
        ppdb_cond_wait(&future->cond, &future->mutex);
    }
    void* result = future->result;
    ppdb_mutex_unlock(&future->mutex);
    return result;
}

bool ppdb_base_future_is_done(ppdb_future_t* future) {
    ppdb_mutex_lock(&future->mutex);
    bool done = (future->state != PPDB_FUTURE_PENDING);
    ppdb_mutex_unlock(&future->mutex);
    return done;
}

const char* ppdb_base_future_get_error(ppdb_future_t* future) {
    ppdb_mutex_lock(&future->mutex);
    const char* error = future->error;
    ppdb_mutex_unlock(&future->mutex);
    return error;
}

//-----------------------------------------------------------------------------
// Future Pattern Implementation
//-----------------------------------------------------------------------------

struct ppdb_base_async_future {
    ppdb_base_async_loop_t* loop;
    bool is_ready;
    void* result;
    size_t result_size;
    ppdb_base_mutex_t* mutex;
    ppdb_base_cond_t* cond;
    ppdb_base_async_cb on_complete;
    void* user_data;
    ppdb_error_t error;
};

ppdb_error_t ppdb_base_future_create(ppdb_base_async_loop_t* loop,
                                    ppdb_base_async_future_t** future) {
    if (!loop || !future) return PPDB_ERR_NULL_POINTER;

    *future = ppdb_base_alloc(sizeof(ppdb_base_async_future_t));
    if (!*future) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*future, 0, sizeof(ppdb_base_async_future_t));
    (*future)->loop = loop;

    ppdb_error_t err = ppdb_base_mutex_create(&(*future)->mutex);
    if (err != PPDB_OK) {
        ppdb_base_free(*future);
        *future = NULL;
        return err;
    }

    err = ppdb_base_cond_create(&(*future)->cond);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy((*future)->mutex);
        ppdb_base_free(*future);
        *future = NULL;
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_base_future_destroy(ppdb_base_async_future_t* future) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    if (future->mutex) {
        ppdb_base_mutex_destroy(future->mutex);
    }

    if (future->cond) {
        ppdb_base_cond_destroy(future->cond);
    }

    if (future->result) {
        ppdb_base_free(future->result);
    }

    ppdb_base_free(future);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_future_set_callback(ppdb_base_async_future_t* future,
                                          ppdb_base_async_cb cb,
                                          void* user_data) {
    if (!future || !cb) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(future->mutex);
    future->on_complete = cb;
    future->user_data = user_data;
    ppdb_base_mutex_unlock(future->mutex);

    return PPDB_OK;
}

ppdb_error_t ppdb_base_future_set_result(ppdb_base_async_future_t* future,
                                        void* result,
                                        size_t size) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(future->mutex);

    if (future->result) {
        ppdb_base_free(future->result);
    }

    if (result && size > 0) {
        future->result = ppdb_base_alloc(size);
        if (!future->result) {
            ppdb_base_mutex_unlock(future->mutex);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memcpy(future->result, result, size);
        future->result_size = size;
    }

    future->is_ready = true;
    future->error = PPDB_OK;

    ppdb_base_cond_broadcast(future->cond);

    if (future->on_complete) {
        future->on_complete((ppdb_base_async_handle_t*)future, 0);
    }

    ppdb_base_mutex_unlock(future->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_future_set_error(ppdb_base_async_future_t* future,
                                       ppdb_error_t error) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(future->mutex);

    future->is_ready = true;
    future->error = error;

    ppdb_base_cond_broadcast(future->cond);

    if (future->on_complete) {
        future->on_complete((ppdb_base_async_handle_t*)future, -1);
    }

    ppdb_base_mutex_unlock(future->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_future_wait(ppdb_base_async_future_t* future) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(future->mutex);
    while (!future->is_ready) {
        ppdb_base_cond_wait(future->cond, future->mutex);
    }
    ppdb_error_t err = future->error;
    ppdb_base_mutex_unlock(future->mutex);

    return err;
}

ppdb_error_t ppdb_base_future_wait_timeout(ppdb_base_async_future_t* future,
                                          uint32_t timeout_ms) {
    if (!future) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(future->mutex);
    
    if (!future->is_ready) {
        ppdb_error_t err = ppdb_base_cond_timedwait(future->cond, future->mutex, timeout_ms);
        if (err != PPDB_OK) {
            ppdb_base_mutex_unlock(future->mutex);
            return err;
        }
    }

    ppdb_error_t err = future->error;
    ppdb_base_mutex_unlock(future->mutex);

    return err;
}

ppdb_error_t ppdb_base_future_is_ready(ppdb_base_async_future_t* future,
                                      bool* ready) {
    if (!future || !ready) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(future->mutex);
    *ready = future->is_ready;
    ppdb_base_mutex_unlock(future->mutex);

    return PPDB_OK;
}

ppdb_error_t ppdb_base_future_get_result(ppdb_base_async_future_t* future,
                                        void* result,
                                        size_t size,
                                        size_t* actual_size) {
    if (!future || !result || !actual_size) return PPDB_ERR_NULL_POINTER;

    ppdb_base_mutex_lock(future->mutex);

    if (!future->is_ready) {
        ppdb_base_mutex_unlock(future->mutex);
        return PPDB_ERR_NOT_READY;
    }

    if (future->error != PPDB_OK) {
        ppdb_base_mutex_unlock(future->mutex);
        return future->error;
    }

    if (!future->result || future->result_size == 0) {
        *actual_size = 0;
        ppdb_base_mutex_unlock(future->mutex);
        return PPDB_OK;
    }

    *actual_size = (size < future->result_size) ? size : future->result_size;
    memcpy(result, future->result, *actual_size);

    ppdb_base_mutex_unlock(future->mutex);
    return PPDB_OK;
}