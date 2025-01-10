/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra_async.c - Unified Asynchronous System Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_sync.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

static void process_io_task(task_node_t* node);
static void process_event_task(task_node_t* node);
static task_node_t* get_task_node(infra_async_context_t* ctx);
static void put_task_node(infra_async_context_t* ctx, task_node_t* node);

//-----------------------------------------------------------------------------
// Internal Types
//-----------------------------------------------------------------------------

#define INFRA_ASYNC_MIN_THREADS 2
#define INFRA_ASYNC_MAX_THREADS 64
#define INFRA_ASYNC_DEFAULT_THREADS 4

typedef struct worker_thread {
    infra_thread_t* thread;
    struct infra_async_context* ctx;
    bool running;
} worker_thread_t;

struct infra_async_context {
    task_node_t* tasks;
    task_node_t* free_nodes;
    infra_mutex_t* lock;
    infra_cond_t* cond;
    bool running;
    bool stop_requested;
    worker_thread_t* workers[INFRA_ASYNC_MAX_THREADS];
    int num_workers;
};

//-----------------------------------------------------------------------------
// Task Pool Management
//-----------------------------------------------------------------------------

static task_node_t* get_task_node(infra_async_context_t* ctx) {
    task_node_t* node;
    
    infra_mutex_lock(ctx->lock);
    if (ctx->free_nodes) {
        node = ctx->free_nodes;
        ctx->free_nodes = node->next;
        infra_mutex_unlock(ctx->lock);
        return node;
    }
    infra_mutex_unlock(ctx->lock);

    node = infra_malloc(sizeof(task_node_t));
    if (!node) {
        return NULL;
    }

    infra_memset(node, 0, sizeof(task_node_t));
    return node;
}

static void put_task_node(infra_async_context_t* ctx, task_node_t* node) {
    infra_mutex_lock(ctx->lock);
    node->next = ctx->free_nodes;
    ctx->free_nodes = node;
    infra_mutex_unlock(ctx->lock);
}

//-----------------------------------------------------------------------------
// Async Context Implementation
//-----------------------------------------------------------------------------

static void* worker_thread_func(void* arg) {
    worker_thread_t* worker = (worker_thread_t*)arg;
    infra_async_context_t* ctx = worker->ctx;

    while (worker->running) {
        task_node_t* node = NULL;

        // 获取任务
        infra_mutex_lock(ctx->lock);
        while (worker->running && !ctx->stop_requested && !ctx->tasks) {
            infra_cond_wait(ctx->cond, ctx->lock);
        }

        if (!worker->running || ctx->stop_requested) {
            infra_mutex_unlock(ctx->lock);
            break;
        }

        if (ctx->tasks) {
            node = ctx->tasks;
            ctx->tasks = node->next;
        }
        infra_mutex_unlock(ctx->lock);

        // 处理任务
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
            } else {
                // 如果任务被取消，调用回调函数通知取消
                node->task.callback(&node->task, INFRA_ERROR_CANCELLED);
            }
            
            // 通知主线程任务已完成
            infra_mutex_lock(ctx->lock);
            infra_cond_broadcast(ctx->cond);
            infra_mutex_unlock(ctx->lock);
            
            // 回收任务节点
            put_task_node(ctx, node);
        }
    }

    return NULL;
}

infra_error_t infra_async_init(infra_async_context_t** ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID;
    }

    infra_async_context_t* new_ctx = infra_malloc(sizeof(infra_async_context_t));
    if (!new_ctx) {
        return INFRA_ERROR_MEMORY;
    }

    infra_memset(new_ctx, 0, sizeof(infra_async_context_t));

    infra_error_t err = infra_mutex_create(&new_ctx->lock);
    if (err != INFRA_OK) {
        infra_free(new_ctx);
        return err;
    }

    err = infra_cond_create(&new_ctx->cond);
    if (err != INFRA_OK) {
        infra_mutex_destroy(new_ctx->lock);
        infra_free(new_ctx);
        return err;
    }

    new_ctx->running = true;
    new_ctx->stop_requested = false;

    // 创建工作线程
    for (int i = 0; i < INFRA_ASYNC_DEFAULT_THREADS; i++) {
        worker_thread_t* worker = infra_malloc(sizeof(worker_thread_t));
        if (!worker) {
            infra_async_destroy(new_ctx);
            return INFRA_ERROR_MEMORY;
        }

        worker->ctx = new_ctx;
        worker->running = true;

        err = infra_thread_create(&worker->thread, worker_thread_func, worker);
        if (err != INFRA_OK) {
            infra_free(worker);
            infra_async_destroy(new_ctx);
            return err;
        }

        new_ctx->workers[new_ctx->num_workers++] = worker;
    }

    *ctx = new_ctx;
    return INFRA_OK;
}

void infra_async_destroy(infra_async_context_t* ctx) {
    if (!ctx) {
        return;
    }

    infra_async_stop(ctx);

    // 清理任务队列
    task_node_t* node = ctx->tasks;
    while (node) {
        task_node_t* next = node->next;
        infra_free(node);
        node = next;
    }

    // 清理空闲节点
    node = ctx->free_nodes;
    while (node) {
        task_node_t* next = node->next;
        infra_free(node);
        node = next;
    }

    infra_cond_destroy(ctx->cond);
    infra_mutex_destroy(ctx->lock);
    infra_free(ctx);
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

    infra_memset(node, 0, sizeof(task_node_t));
    infra_memcpy(&node->task, task, sizeof(infra_async_task_t));
    node->next = NULL;  // 新节点总是添加到尾部

    infra_mutex_lock(ctx->lock);
    if (!ctx->tasks) {
        ctx->tasks = node;
    } else {
        task_node_t* tail = ctx->tasks;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = node;
    }
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
        if (infra_memcmp(&node->task, task, sizeof(infra_async_task_t)) == 0) {
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
    if (!ctx->running) {
        infra_mutex_unlock(ctx->lock);
        return INFRA_ERROR_BUSY;
    }

    // 如果有任务，等待它们完成
    if (ctx->tasks) {
        uint64_t start_time = infra_time_monotonic();
        uint64_t end_time = timeout_ms == UINT64_MAX ? UINT64_MAX : start_time + timeout_ms;

        while (ctx->tasks && !ctx->stop_requested) {
            if (timeout_ms != UINT64_MAX) {
                uint64_t now = infra_time_monotonic();
                if (now >= end_time) {
                    infra_mutex_unlock(ctx->lock);
                    return INFRA_ERROR_TIMEOUT;
                }
                uint64_t remaining = end_time - now;
                infra_error_t err = infra_cond_timedwait(ctx->cond, ctx->lock, remaining);
                if (err == INFRA_ERROR_TIMEOUT) {
                    infra_mutex_unlock(ctx->lock);
                    return INFRA_ERROR_TIMEOUT;
                }
            } else {
                infra_cond_wait(ctx->cond, ctx->lock);
            }
        }
    }

    infra_mutex_unlock(ctx->lock);
    return INFRA_OK;
}

infra_error_t infra_async_stop(infra_async_context_t* ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID;
    }

    infra_mutex_lock(ctx->lock);
    ctx->stop_requested = true;
    ctx->running = false;
    infra_cond_broadcast(ctx->cond);
    infra_mutex_unlock(ctx->lock);

    // 等待所有工作线程结束
    for (int i = 0; i < ctx->num_workers; i++) {
        worker_thread_t* worker = ctx->workers[i];
        if (worker) {
            worker->running = false;
            infra_thread_join(worker->thread);
            infra_free(worker);
            ctx->workers[i] = NULL;
        }
    }
    ctx->num_workers = 0;

    return INFRA_OK;
}
