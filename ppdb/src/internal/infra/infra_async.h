/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_async.h - Unified Asynchronous System Interface
 */

#ifndef INFRA_ASYNC_H
#define INFRA_ASYNC_H

#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"

//-----------------------------------------------------------------------------
// Types and Constants
//-----------------------------------------------------------------------------

typedef struct infra_async_context infra_async_context_t;
typedef struct infra_async_task infra_async_task_t;
typedef void (*infra_async_callback_t)(infra_async_task_t* task, infra_error_t error);

typedef enum infra_async_type {
    INFRA_ASYNC_READ,
    INFRA_ASYNC_WRITE,
    INFRA_ASYNC_ACCEPT,
    INFRA_ASYNC_CONNECT,
    INFRA_ASYNC_TIMER,
    INFRA_ASYNC_EVENT
} infra_async_type_t;

struct infra_async_task {
    infra_async_type_t type;
    infra_async_callback_t callback;
    void* user_data;
    infra_stats_t* stats;
    union {
        struct {
            int fd;
            void* buffer;
            size_t size;
            size_t offset;
        } io;
        struct {
            int fd;
            struct sockaddr* addr;
            socklen_t* addrlen;
        } accept;
        struct {
            int fd;
            const struct sockaddr* addr;
            socklen_t addrlen;
        } connect;
        struct {
            uint64_t timeout_ms;
            bool periodic;
        } timer;
        struct {
            int event_fd;
            uint64_t value;
        } event;
    };
};

//-----------------------------------------------------------------------------
// Internal Types
//-----------------------------------------------------------------------------

typedef struct task_node {
    infra_async_task_t task;
    struct task_node* next;
    bool cancelled;
} task_node_t;

//-----------------------------------------------------------------------------
// Async Task Management
//-----------------------------------------------------------------------------

infra_error_t infra_async_init(infra_async_context_t** ctx);
void infra_async_destroy(infra_async_context_t* ctx);
infra_error_t infra_async_submit(infra_async_context_t* ctx,
                                infra_async_task_t* task);
infra_error_t infra_async_cancel(infra_async_context_t* ctx,
                                infra_async_task_t* task);
infra_error_t infra_async_run(infra_async_context_t* ctx, uint64_t timeout_ms);
infra_error_t infra_async_stop(infra_async_context_t* ctx);

#endif /* INFRA_ASYNC_H */ 