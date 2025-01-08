/*
 * base_io.inc.c - Base IO Manager Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// IO manager thread function
static void ppdb_base_io_thread(void* arg) {
    ppdb_base_io_manager_t* mgr = (ppdb_base_io_manager_t*)arg;
    while (mgr->running) {
        // Lock IO manager
        ppdb_base_mutex_lock(mgr->mutex);

        // Process IO requests
        struct ppdb_base_io_request_s* req = mgr->requests;
        if (req) {
            // Remove request from queue
            mgr->requests = req->next;
            
            // Unlock IO manager before processing request
            ppdb_base_mutex_unlock(mgr->mutex);
            
            // Process request
            if (req->func) {
                req->func(req->arg);
            }
            
            // Free request
            free(req);
        } else {
            // No requests, unlock and sleep
            ppdb_base_mutex_unlock(mgr->mutex);
            ppdb_base_sleep(1);  // Sleep for 1ms
        }
    }
}

// Create IO manager
ppdb_error_t ppdb_base_io_manager_create(ppdb_base_io_manager_t** mgr) {
    if (!mgr) return PPDB_BASE_ERR_PARAM;

    // Allocate IO manager
    ppdb_base_io_manager_t* new_mgr = malloc(sizeof(ppdb_base_io_manager_t));
    if (!new_mgr) return PPDB_BASE_ERR_MEMORY;

    // Initialize IO manager
    new_mgr->mutex = NULL;
    new_mgr->worker = NULL;
    new_mgr->requests = NULL;
    new_mgr->running = false;

    // Create mutex
    ppdb_error_t err = ppdb_base_mutex_create(&new_mgr->mutex);
    if (err != PPDB_OK) {
        free(new_mgr);
        return err;
    }

    // Start worker thread
    new_mgr->running = true;
    err = ppdb_base_thread_create(&new_mgr->worker, ppdb_base_io_thread, new_mgr);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(new_mgr->mutex);
        free(new_mgr);
        return err;
    }

    *mgr = new_mgr;
    return PPDB_OK;
}

// Destroy IO manager
ppdb_error_t ppdb_base_io_manager_destroy(ppdb_base_io_manager_t* mgr) {
    if (!mgr) return PPDB_BASE_ERR_PARAM;

    // Stop worker thread
    if (mgr->running) {
        mgr->running = false;
        if (mgr->worker) {
            ppdb_base_thread_join(mgr->worker);
            ppdb_base_thread_destroy(mgr->worker);
        }
    }

    // Free pending requests
    ppdb_base_mutex_lock(mgr->mutex);
    while (mgr->requests) {
        struct ppdb_base_io_request_s* req = mgr->requests;
        mgr->requests = req->next;
        free(req);
    }
    ppdb_base_mutex_unlock(mgr->mutex);

    // Destroy mutex
    if (mgr->mutex) {
        ppdb_base_mutex_destroy(mgr->mutex);
    }

    // Free IO manager
    free(mgr);
    return PPDB_OK;
}

// Process IO requests
ppdb_error_t ppdb_base_io_manager_process(ppdb_base_io_manager_t* mgr) {
    if (!mgr) return PPDB_BASE_ERR_PARAM;

    // Lock IO manager
    ppdb_error_t err = ppdb_base_mutex_lock(mgr->mutex);
    if (err != PPDB_OK) return err;

    // Process all pending requests
    while (mgr->requests) {
        // Get next request
        struct ppdb_base_io_request_s* req = mgr->requests;
        mgr->requests = req->next;

        // Unlock IO manager before processing request
        ppdb_base_mutex_unlock(mgr->mutex);

        // Process request
        if (req->func) {
            req->func(req->arg);
        }

        // Free request
        free(req);

        // Lock IO manager for next iteration
        err = ppdb_base_mutex_lock(mgr->mutex);
        if (err != PPDB_OK) return err;
    }

    // Unlock IO manager
    ppdb_base_mutex_unlock(mgr->mutex);
    return PPDB_OK;
}

// Schedule IO request
ppdb_error_t ppdb_base_io_manager_schedule(ppdb_base_io_manager_t* mgr,
                                         ppdb_base_io_func_t func,
                                         void* arg) {
    if (!mgr || !func) return PPDB_BASE_ERR_PARAM;

    // Create request
    struct ppdb_base_io_request_s* req = malloc(sizeof(struct ppdb_base_io_request_s));
    if (!req) return PPDB_BASE_ERR_MEMORY;

    // Initialize request
    req->func = func;
    req->arg = arg;
    req->next = NULL;

    // Add request to queue
    ppdb_base_mutex_lock(mgr->mutex);
    if (!mgr->requests) {
        mgr->requests = req;
    } else {
        struct ppdb_base_io_request_s* last = mgr->requests;
        while (last->next) {
            last = last->next;
        }
        last->next = req;
    }
    ppdb_base_mutex_unlock(mgr->mutex);

    return PPDB_OK;
} 