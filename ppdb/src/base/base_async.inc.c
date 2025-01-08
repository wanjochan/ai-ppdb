/*
 * base_async.inc.c - Async Operations Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Worker thread function
static void async_worker_thread(void* arg) {
    ppdb_base_async_loop_t* loop = (ppdb_base_async_loop_t*)arg;
    struct ppdb_base_async_task_s* task;

    while (loop->running) {
        ppdb_base_mutex_lock(loop->mutex);
        task = loop->tasks;
        if (task) {
            loop->tasks = task->next;
        }
        ppdb_base_mutex_unlock(loop->mutex);

        if (task) {
            task->func(task->arg);
            free(task);
        } else {
            ppdb_base_sleep(1); // Sleep for 1ms when no tasks
        }
    }
}

// Initialize async loop
ppdb_error_t ppdb_base_async_init(ppdb_base_async_loop_t** loop) {
    if (!loop) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_async_loop_t* new_loop = (ppdb_base_async_loop_t*)malloc(sizeof(ppdb_base_async_loop_t));
    if (!new_loop) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_loop, 0, sizeof(ppdb_base_async_loop_t));

    // Create mutex
    ppdb_error_t err = ppdb_base_mutex_create(&new_loop->mutex);
    if (err != PPDB_OK) {
        free(new_loop);
        return err;
    }

    // Create worker thread
    new_loop->running = true;
    err = ppdb_base_thread_create(&new_loop->worker, async_worker_thread, new_loop);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(new_loop->mutex);
        free(new_loop);
        return err;
    }

    *loop = new_loop;
    return PPDB_OK;
}

// Cleanup async loop
ppdb_error_t ppdb_base_async_cleanup(ppdb_base_async_loop_t* loop) {
    if (!loop) {
        return PPDB_BASE_ERR_PARAM;
    }

    // Stop worker thread
    loop->running = false;
    if (loop->worker) {
        ppdb_base_thread_join(loop->worker);
        free(loop->worker);
    }

    // Clean up remaining tasks
    struct ppdb_base_async_task_s* task = loop->tasks;
    while (task) {
        struct ppdb_base_async_task_s* next = task->next;
        free(task);
        task = next;
    }

    // Clean up mutex
    if (loop->mutex) {
        ppdb_base_mutex_destroy(loop->mutex);
    }

    free(loop);
    return PPDB_OK;
}

// Submit async task
ppdb_error_t ppdb_base_async_submit(ppdb_base_async_loop_t* loop, ppdb_base_async_func_t func, void* arg) {
    if (!loop || !func) {
        return PPDB_BASE_ERR_PARAM;
    }

    struct ppdb_base_async_task_s* task = (struct ppdb_base_async_task_s*)malloc(sizeof(struct ppdb_base_async_task_s));
    if (!task) {
        return PPDB_BASE_ERR_MEMORY;
    }

    task->func = func;
    task->arg = arg;
    task->next = NULL;

    ppdb_base_mutex_lock(loop->mutex);
    if (!loop->tasks) {
        loop->tasks = task;
    } else {
        struct ppdb_base_async_task_s* current = loop->tasks;
        while (current->next) {
            current = current->next;
        }
        current->next = task;
    }
    ppdb_base_mutex_unlock(loop->mutex);

    return PPDB_OK;
}

ppdb_error_t ppdb_base_async_schedule(ppdb_base_t* base, ppdb_base_async_func_t fn, void* arg, ppdb_base_async_handle_t** handle) {
    if (!base || !fn || !handle) return PPDB_BASE_ERR_PARAM;

    // Create async handle
    ppdb_base_async_handle_t* new_handle = (ppdb_base_async_handle_t*)malloc(sizeof(ppdb_base_async_handle_t));
    if (!new_handle) return PPDB_BASE_ERR_MEMORY;

    // Initialize handle
    new_handle->fn = fn;
    new_handle->arg = arg;
    new_handle->next = NULL;
    new_handle->cancelled = false;

    // TODO: Add to async task queue and schedule execution
    // For now, just execute immediately
    fn(arg);

    *handle = new_handle;
    return PPDB_OK;
}

// Cancel async operation
ppdb_error_t ppdb_base_async_cancel(ppdb_base_async_handle_t* handle) {
    if (!handle) return PPDB_BASE_ERR_PARAM;
    handle->cancelled = true;
    return PPDB_OK;
}