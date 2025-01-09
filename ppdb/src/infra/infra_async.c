/*
 * infra_async.c - Asynchronous System Implementation
 *
 * This file contains:
 * 1. Async Loop Management
 * 2. Task Queue Management
 * 3. Asynchronous IO Operations
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_async.h"

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static void task_complete(ppdb_async_handle_t* handle, ppdb_error_t error) {
    handle->state = PPDB_ASYNC_STATE_COMPLETED;
    if (handle->callback) {
        handle->callback(error, handle->callback_arg);
    }
}

//-----------------------------------------------------------------------------
// Async Loop Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_async_loop_create(ppdb_async_loop_t** loop) {
    if (!loop) {
        return PPDB_ERR_PARAM;
    }

    void* loop_ptr;
    ppdb_error_t err = ppdb_mem_malloc(sizeof(ppdb_async_loop_t), &loop_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_async_loop_t* new_loop = (ppdb_async_loop_t*)loop_ptr;

    // Initialize lock and condition variable
    err = ppdb_mutex_create(&new_loop->lock);
    if (err != PPDB_OK) {
        ppdb_mem_free(new_loop);
        return err;
    }

    err = ppdb_cond_create(&new_loop->cond);
    if (err != PPDB_OK) {
        ppdb_mutex_destroy(new_loop->lock);
        ppdb_mem_free(new_loop);
        return err;
    }

    // Initialize queues
    for (int i = 0; i < 3; i++) {
        new_loop->queues[i].head = NULL;
        new_loop->queues[i].tail = NULL;
        new_loop->queues[i].size = 0;
    }

    // Initialize IO statistics
    memset(&new_loop->io_stats, 0, sizeof(ppdb_async_io_stats_t));

    new_loop->running = true;

    *loop = new_loop;
    return PPDB_OK;
}

ppdb_error_t ppdb_async_loop_destroy(ppdb_async_loop_t* loop) {
    if (!loop) {
        return PPDB_ERR_PARAM;
    }

    // Stop the loop
    loop->running = false;

    // Free all tasks in queues
    for (int i = 0; i < 3; i++) {
        ppdb_async_handle_t* handle = loop->queues[i].head;
        while (handle) {
            ppdb_async_handle_t* next = handle->next;
            ppdb_mem_free(handle);
            handle = next;
        }
    }

    // Destroy synchronization primitives
    ppdb_cond_destroy(loop->cond);
    ppdb_mutex_destroy(loop->lock);

    ppdb_mem_free(loop);
    return PPDB_OK;
}

ppdb_error_t ppdb_async_loop_run(ppdb_async_loop_t* loop, uint32_t timeout_ms) {
    if (!loop) {
        return PPDB_ERR_PARAM;
    }

    uint64_t start_time = get_time_us();
    uint64_t current_time;
    ppdb_error_t err;

    while (loop->running) {
        err = ppdb_mutex_lock(loop->lock);
        if (err != PPDB_OK) {
            return err;
        }

        // Check timeout
        if (timeout_ms > 0) {
            current_time = get_time_us();
            if ((current_time - start_time) / 1000 >= timeout_ms) {
                ppdb_mutex_unlock(loop->lock);
                return PPDB_OK;
            }
        }

        // Process ready queue
        ppdb_async_handle_t* handle = loop->queues[0].head;
        if (handle) {
            // Move to running queue
            loop->queues[0].head = handle->next;
            if (!loop->queues[0].head) {
                loop->queues[0].tail = NULL;
            }
            loop->queues[0].size--;

            handle->next = NULL;
            if (!loop->queues[1].head) {
                loop->queues[1].head = loop->queues[1].tail = handle;
            } else {
                loop->queues[1].tail->next = handle;
                loop->queues[1].tail = handle;
            }
            loop->queues[1].size++;

            // Execute task
            handle->state = PPDB_ASYNC_STATE_RUNNING;
            handle->start_time = get_time_us();
            
            // Release lock while executing task
            ppdb_mutex_unlock(loop->lock);
            
            // Execute the task
            handle->func(handle->func_arg);
            
            // Reacquire lock
            err = ppdb_mutex_lock(loop->lock);
            if (err != PPDB_OK) {
                return err;
            }

            // Move to completed queue
            handle->complete_time = get_time_us();
            task_complete(handle, PPDB_OK);

            // Remove from running queue
            if (handle == loop->queues[1].head) {
                loop->queues[1].head = handle->next;
                if (!loop->queues[1].head) {
                    loop->queues[1].tail = NULL;
                }
            }
            loop->queues[1].size--;

            // Add to completed queue
            handle->next = NULL;
            if (!loop->queues[2].head) {
                loop->queues[2].head = loop->queues[2].tail = handle;
            } else {
                loop->queues[2].tail->next = handle;
                loop->queues[2].tail = handle;
            }
            loop->queues[2].size++;

            // Clean up completed tasks
            ppdb_async_handle_t* completed = loop->queues[2].head;
            while (completed) {
                ppdb_async_handle_t* next = completed->next;
                if (completed->state == PPDB_ASYNC_STATE_COMPLETED) {
                    if (completed == loop->queues[2].head) {
                        loop->queues[2].head = next;
                    }
                    if (completed == loop->queues[2].tail) {
                        loop->queues[2].tail = NULL;
                    }
                    loop->queues[2].size--;
                    ppdb_mem_free(completed);
                }
                completed = next;
            }

            ppdb_mutex_unlock(loop->lock);
            continue;
        }

        // No tasks in ready queue
        if (timeout_ms == 0) {
            ppdb_mutex_unlock(loop->lock);
            break;
        }

        // Wait for new tasks with timeout
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        
        err = ppdb_cond_timedwait(loop->cond, loop->lock, &ts);
        ppdb_mutex_unlock(loop->lock);
        if (err != PPDB_OK && err != PPDB_ERR_TIMEOUT) {
            return err;
        }
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_async_loop_stop(ppdb_async_loop_t* loop) {
    if (!loop) {
        return PPDB_ERR_PARAM;
    }

    ppdb_error_t err = ppdb_mutex_lock(loop->lock);
    if (err != PPDB_OK) {
        return err;
    }

    loop->running = false;
    ppdb_cond_signal(loop->cond);

    ppdb_mutex_unlock(loop->lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_async_submit(ppdb_async_loop_t* loop,
                              ppdb_async_func_t func,
                              void* func_arg,
                              uint32_t flags,
                              uint32_t timeout_ms,
                              ppdb_async_callback_t callback,
                              void* callback_arg,
                              ppdb_async_handle_t** handle) {
    if (!loop || !func || !handle) {
        return PPDB_ERR_PARAM;
    }

    void* handle_ptr;
    ppdb_error_t err = ppdb_mem_malloc(sizeof(ppdb_async_handle_t), &handle_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_async_handle_t* new_handle = (ppdb_async_handle_t*)handle_ptr;

    new_handle->loop = loop;
    new_handle->state = PPDB_ASYNC_STATE_INIT;
    new_handle->func = func;
    new_handle->func_arg = func_arg;
    new_handle->callback = callback;
    new_handle->callback_arg = callback_arg;
    new_handle->next = NULL;
    new_handle->submit_time = get_time_us();
    new_handle->start_time = 0;
    new_handle->complete_time = 0;

    err = ppdb_mutex_lock(loop->lock);
    if (err != PPDB_OK) {
        ppdb_mem_free(new_handle);
        return err;
    }

    // Add to ready queue
    new_handle->state = PPDB_ASYNC_STATE_QUEUED;
    if (!loop->queues[0].head) {
        loop->queues[0].head = loop->queues[0].tail = new_handle;
    } else {
        loop->queues[0].tail->next = new_handle;
        loop->queues[0].tail = new_handle;
    }
    loop->queues[0].size++;

    ppdb_cond_signal(loop->cond);
    ppdb_mutex_unlock(loop->lock);

    *handle = new_handle;
    return PPDB_OK;
}

ppdb_error_t ppdb_async_cancel(ppdb_async_handle_t* handle) {
    if (!handle || !handle->loop) {
        return PPDB_ERR_PARAM;
    }

    ppdb_async_loop_t* loop = handle->loop;
    ppdb_error_t err = ppdb_mutex_lock(loop->lock);
    if (err != PPDB_OK) {
        return err;
    }

    // Only cancel if in ready queue
    if (handle->state == PPDB_ASYNC_STATE_QUEUED) {
        ppdb_async_handle_t* prev = NULL;
        ppdb_async_handle_t* curr = loop->queues[0].head;

        while (curr && curr != handle) {
            prev = curr;
            curr = curr->next;
        }

        if (curr) {
            if (prev) {
                prev->next = curr->next;
            } else {
                loop->queues[0].head = curr->next;
            }

            if (!curr->next) {
                loop->queues[0].tail = prev;
            }

            loop->queues[0].size--;
            handle->state = PPDB_ASYNC_STATE_CANCELLED;
            handle->complete_time = get_time_us();

            if (handle->callback) {
                handle->callback(PPDB_ERR_CANCEL, handle->callback_arg);
            }
        }
    }

    ppdb_mutex_unlock(loop->lock);
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// IO Operations
//-----------------------------------------------------------------------------

typedef struct {
    ppdb_async_handle_t* handle;
    int fd;
    void* buf;
    size_t count;
    uint64_t offset;
    ppdb_async_callback_t callback;
    void* user_data;
    uint64_t submit_time;
    uint64_t start_time;
    uint64_t complete_time;
} ppdb_async_io_request_t;

static void async_io_handler(void* arg) {
    ppdb_async_io_request_t* req = (ppdb_async_io_request_t*)arg;
    ssize_t ret = 0;
    int saved_errno = 0;

    req->start_time = get_time_us();

    // Special offset handling
    if (req->offset == (uint64_t)-2) {  // fsync
        ret = fsync(req->fd);
    } else if (req->offset == (uint64_t)-1) {  // append
        if (req->buf) {
            ret = write(req->fd, req->buf, req->count);
        } else {
            ret = read(req->fd, req->buf, req->count);
        }
    } else {  // normal read/write with offset
        if (req->buf) {
            ret = pwrite(req->fd, req->buf, req->count, req->offset);
        } else {
            ret = pread(req->fd, req->buf, req->count, req->offset);
        }
    }

    saved_errno = errno;
    req->complete_time = get_time_us();

    if (ret < 0) {
        if (req->callback) {
            req->callback(PPDB_ERR_IO, req->user_data);
        }
    } else {
        if (req->callback) {
            req->callback(PPDB_OK, req->user_data);
        }
    }

    ppdb_mem_free(req);
}

ppdb_error_t ppdb_async_read(ppdb_async_loop_t* loop,
                            int fd,
                            void* buf,
                            size_t count,
                            uint64_t offset,
                            ppdb_async_callback_t callback,
                            void* user_data) {
    if (!loop || fd < 0 || !buf || count == 0) {
        return PPDB_ERR_PARAM;
    }

    void* req_ptr;
    ppdb_error_t err = ppdb_mem_malloc(sizeof(ppdb_async_io_request_t), &req_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_async_io_request_t* req = (ppdb_async_io_request_t*)req_ptr;

    req->fd = fd;
    req->buf = buf;
    req->count = count;
    req->offset = offset;
    req->callback = callback;
    req->user_data = user_data;
    req->submit_time = get_time_us();

    ppdb_async_handle_t* handle;
    err = ppdb_async_submit(loop, async_io_handler, req, 0, 0, NULL, NULL, &handle);
    if (err != PPDB_OK) {
        ppdb_mem_free(req);
        return err;
    }

    req->handle = handle;
    return PPDB_OK;
}

ppdb_error_t ppdb_async_write(ppdb_async_loop_t* loop,
                             int fd,
                             const void* buf,
                             size_t count,
                             uint64_t offset,
                             ppdb_async_callback_t callback,
                             void* user_data) {
    if (!loop || fd < 0 || !buf || count == 0) {
        return PPDB_ERR_PARAM;
    }

    // Create IO request
    void* req_ptr;
    ppdb_error_t err = ppdb_mem_malloc(sizeof(ppdb_async_io_request_t), &req_ptr);
    if (err != PPDB_OK) {
        return err;
    }

    ppdb_async_io_request_t* req = (ppdb_async_io_request_t*)req_ptr;
    req->fd = fd;
    req->buf = (void*)buf;
    req->count = count;
    req->offset = offset;
    req->callback = callback;
    req->user_data = user_data;
    req->submit_time = get_time_us();

    // Submit IO task
    err = ppdb_async_submit(loop, async_io_handler, req, 0, 0,
                           NULL, NULL, &req->handle);
    if (err != PPDB_OK) {
        ppdb_mem_free(req);
        return err;
    }

    // Update statistics
    ppdb_mutex_lock(loop->lock);
    loop->io_stats.write_requests++;
    loop->io_stats.bytes_written += count;
    ppdb_mutex_unlock(loop->lock);

    return PPDB_OK;
}

ppdb_error_t ppdb_async_fsync(ppdb_async_loop_t* loop,
                             int fd,
                             ppdb_async_callback_t callback,
                             void* user_data) {
    if (!loop || fd < 0) {
        return PPDB_ERR_PARAM;
    }

    void* req_ptr;
    ppdb_error_t err = ppdb_mem_malloc(sizeof(ppdb_async_io_request_t), &req_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_async_io_request_t* req = (ppdb_async_io_request_t*)req_ptr;

    req->fd = fd;
    req->buf = NULL;
    req->count = 0;
    req->offset = -2;  // Indicate fsync operation
    req->callback = callback;
    req->user_data = user_data;
    req->submit_time = get_time_us();

    ppdb_async_handle_t* handle;
    err = ppdb_async_submit(loop, async_io_handler, req, 0, 0, NULL, NULL, &handle);
    if (err != PPDB_OK) {
        ppdb_mem_free(req);
        return err;
    }

    req->handle = handle;
    return PPDB_OK;
}

void ppdb_async_get_io_stats(ppdb_async_loop_t* loop,
                            ppdb_async_io_stats_t* stats) {
    if (!loop || !stats) {
        return;
    }

    ppdb_mutex_lock(loop->lock);
    *stats = loop->io_stats;
    ppdb_mutex_unlock(loop->lock);
}
