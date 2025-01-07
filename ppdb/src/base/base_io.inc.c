/*
 * base_io.inc.c - IO Operations Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Worker thread function
static void io_worker_thread(void* arg) {
    ppdb_base_io_manager_t* manager = (ppdb_base_io_manager_t*)arg;
    struct ppdb_base_io_request_s* request;

    while (manager->running) {
        ppdb_base_mutex_lock(manager->mutex);
        request = manager->requests;
        if (request) {
            manager->requests = request->next;
        }
        ppdb_base_mutex_unlock(manager->mutex);

        if (request) {
            request->func(request->arg);
            free(request);
        } else {
            ppdb_base_sleep(1); // Sleep for 1ms when no requests
        }
    }
}

// Create IO manager
ppdb_error_t ppdb_base_io_manager_create(ppdb_base_io_manager_t** manager) {
    if (!manager) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_io_manager_t* new_manager = (ppdb_base_io_manager_t*)malloc(sizeof(ppdb_base_io_manager_t));
    if (!new_manager) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_manager, 0, sizeof(ppdb_base_io_manager_t));

    // Create mutex
    ppdb_error_t err = ppdb_base_mutex_create(&new_manager->mutex);
    if (err != PPDB_OK) {
        free(new_manager);
        return err;
    }

    // Create worker thread
    new_manager->running = true;
    err = ppdb_base_thread_create(&new_manager->worker, io_worker_thread, new_manager);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(new_manager->mutex);
        free(new_manager);
        return err;
    }

    *manager = new_manager;
    return PPDB_OK;
}

// Destroy IO manager
ppdb_error_t ppdb_base_io_manager_destroy(ppdb_base_io_manager_t* manager) {
    if (!manager) {
        return PPDB_BASE_ERR_PARAM;
    }

    // Stop worker thread
    manager->running = false;
    if (manager->worker) {
        ppdb_base_thread_join(manager->worker);
        free(manager->worker);
    }

    // Clean up remaining requests
    struct ppdb_base_io_request_s* request = manager->requests;
    while (request) {
        struct ppdb_base_io_request_s* next = request->next;
        free(request);
        request = next;
    }

    // Clean up mutex
    if (manager->mutex) {
        ppdb_base_mutex_destroy(manager->mutex);
    }

    free(manager);
    return PPDB_OK;
}

// Submit IO request
ppdb_error_t ppdb_base_io_submit(ppdb_base_io_manager_t* manager, ppdb_base_io_func_t func, void* arg) {
    if (!manager || !func) {
        return PPDB_BASE_ERR_PARAM;
    }

    struct ppdb_base_io_request_s* request = (struct ppdb_base_io_request_s*)malloc(sizeof(struct ppdb_base_io_request_s));
    if (!request) {
        return PPDB_BASE_ERR_MEMORY;
    }

    request->func = func;
    request->arg = arg;
    request->next = NULL;

    ppdb_base_mutex_lock(manager->mutex);
    if (!manager->requests) {
        manager->requests = request;
    } else {
        struct ppdb_base_io_request_s* current = manager->requests;
        while (current->next) {
            current = current->next;
        }
        current->next = request;
    }
    ppdb_base_mutex_unlock(manager->mutex);

    return PPDB_OK;
} 