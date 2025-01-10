/*
 * infra_async.c - Unified Asynchronous System Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Internal Types
//-----------------------------------------------------------------------------

#define MAX_TASKS 1024

typedef struct task_node {
    infra_async_task_t task;
    struct task_node* next;
    bool cancelled;
} task_node_t;

struct infra_async_context {
    infra_mutex_t* lock;
    infra_cond_t* cond;
    task_node_t* tasks;
    task_node_t* free_list;
    bool running;
    bool stop_requested;
    infra_stats_t stats;
};

//-----------------------------------------------------------------------------
// Task Pool Management
//-----------------------------------------------------------------------------

static task_node_t* get_task_node(infra_async_context_t* ctx) {
    task_node_t* node;
    
    infra_mutex_lock(ctx->lock);
    if (ctx->free_list) {
        node = ctx->free_list;
        ctx->free_list = node->next;
        infra_mutex_unlock(ctx->lock);
        return node;
    }
    infra_mutex_unlock(ctx->lock);

    node = malloc(sizeof(task_node_t));
    if (!node) {
        return NULL;
    }
    return node;
}

static void put_task_node(infra_async_context_t* ctx, task_node_t* node) {
    infra_mutex_lock(ctx->lock);
    node->next = ctx->free_list;
    ctx->free_list = node;
    infra_mutex_unlock(ctx->lock);
}

//-----------------------------------------------------------------------------
// Async Context Implementation
//-----------------------------------------------------------------------------

infra_error_t infra_async_init(infra_async_context_t** ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID;
    }

    infra_async_context_t* new_ctx = malloc(sizeof(infra_async_context_t));
    if (!new_ctx) {
        return INFRA_ERROR_MEMORY;
    }

    memset(new_ctx, 0, sizeof(infra_async_context_t));

    infra_error_t err = infra_mutex_create(&new_ctx->lock);
    if (err != INFRA_OK) {
        free(new_ctx);
        return err;
    }

    err = infra_cond_create(&new_ctx->cond);
    if (err != INFRA_OK) {
        infra_mutex_destroy(new_ctx->lock);
        free(new_ctx);
        return err;
    }

    *ctx = new_ctx;
    return INFRA_OK;
}

void infra_async_destroy(infra_async_context_t* ctx) {
    if (!ctx) {
        return;
    }

    // Stop if running
    if (ctx->running) {
        infra_async_stop(ctx);
    }

    // Free all task nodes
    task_node_t* node = ctx->tasks;
    while (node) {
        task_node_t* next = node->next;
        free(node);
        node = next;
    }

    node = ctx->free_list;
    while (node) {
        task_node_t* next = node->next;
        free(node);
        node = next;
    }

    infra_cond_destroy(ctx->cond);
    infra_mutex_destroy(ctx->lock);
    free(ctx);
}

infra_error_t infra_async_submit(infra_async_context_t* ctx,
                                infra_async_task_t* task) {
    if (!ctx || !task) {
        return INFRA_ERROR_INVALID;
    }

    task_node_t* node = get_task_node(ctx);
    if (!node) {
        return INFRA_ERROR_MEMORY;
    }

    memset(node, 0, sizeof(task_node_t));
    memcpy(&node->task, task, sizeof(infra_async_task_t));

    infra_mutex_lock(ctx->lock);
    node->next = ctx->tasks;
    ctx->tasks = node;
    infra_cond_signal(ctx->cond);
    infra_mutex_unlock(ctx->lock);

    return INFRA_OK;
}

infra_error_t infra_async_cancel(infra_async_context_t* ctx,
                                infra_async_task_t* task) {
    if (!ctx || !task) {
        return INFRA_ERROR_INVALID;
    }

    infra_mutex_lock(ctx->lock);
    task_node_t* node = ctx->tasks;
    while (node) {
        if (memcmp(&node->task, task, sizeof(infra_async_task_t)) == 0) {
            node->cancelled = true;
            break;
        }
        node = node->next;
    }
    infra_mutex_unlock(ctx->lock);

    return INFRA_OK;
}

static void process_io_task(task_node_t* node) {
    infra_async_task_t* task = &node->task;
    ssize_t ret;

    switch (task->type) {
        case INFRA_ASYNC_READ:
            ret = read(task->io.fd, task->io.buffer, task->io.size);
            if (ret < 0) {
                task->callback(task, INFRA_ERROR_IO);
            } else {
                task->io.offset = ret;
                task->callback(task, INFRA_OK);
            }
            break;

        case INFRA_ASYNC_WRITE:
            ret = write(task->io.fd, task->io.buffer, task->io.size);
            if (ret < 0) {
                task->callback(task, INFRA_ERROR_IO);
            } else {
                task->io.offset = ret;
                task->callback(task, INFRA_OK);
            }
            break;

        default:
            task->callback(task, INFRA_ERROR_INVALID);
            break;
    }
}

static void process_event_task(task_node_t* node) {
    infra_async_task_t* task = &node->task;
    
    if (task->event.event_fd >= 0) {
        uint64_t value;
        ssize_t ret = read(task->event.event_fd, &value, sizeof(value));
        if (ret < 0) {
            task->callback(task, INFRA_ERROR_IO);
            return;
        }
    }

    task->callback(task, INFRA_OK);
}

infra_error_t infra_async_run(infra_async_context_t* ctx, uint64_t timeout_ms) {
    if (!ctx) {
        return INFRA_ERROR_INVALID;
    }

    infra_mutex_lock(ctx->lock);
    if (ctx->running) {
        infra_mutex_unlock(ctx->lock);
        return INFRA_ERROR_BUSY;
    }
    ctx->running = true;
    ctx->stop_requested = false;
    infra_mutex_unlock(ctx->lock);

    uint64_t start_time = infra_time_monotonic();
    bool timeout = false;

    while (!ctx->stop_requested && !timeout) {
        task_node_t* node = NULL;

        infra_mutex_lock(ctx->lock);
        if (!ctx->tasks) {
            if (timeout_ms > 0) {
                infra_cond_timedwait(ctx->cond, ctx->lock, timeout_ms);
            } else {
                infra_cond_wait(ctx->cond, ctx->lock);
            }
        }

        if (ctx->tasks) {
            node = ctx->tasks;
            ctx->tasks = node->next;
        }
        infra_mutex_unlock(ctx->lock);

        if (node) {
            if (!node->cancelled) {
                switch (node->task.type) {
                    case INFRA_ASYNC_READ:
                    case INFRA_ASYNC_WRITE:
                        process_io_task(node);
                        break;

                    case INFRA_ASYNC_EVENT:
                        process_event_task(node);
                        break;

                    default:
                        node->task.callback(&node->task, INFRA_ERROR_INVALID);
                        break;
                }
            }
            put_task_node(ctx, node);
        }

        if (timeout_ms > 0) {
            uint64_t current_time = infra_time_monotonic();
            if (current_time - start_time >= timeout_ms) {
                timeout = true;
            }
        }
    }

    infra_mutex_lock(ctx->lock);
    ctx->running = false;
    infra_mutex_unlock(ctx->lock);

    return INFRA_OK;
}

infra_error_t infra_async_stop(infra_async_context_t* ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID;
    }

    infra_mutex_lock(ctx->lock);
    ctx->stop_requested = true;
    infra_cond_signal(ctx->cond);
    infra_mutex_unlock(ctx->lock);

    return INFRA_OK;
}
