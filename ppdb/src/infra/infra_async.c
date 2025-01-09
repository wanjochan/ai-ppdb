#include "cosmopolitan.h"
#include "internal/infra/infra.h"

/* Task Queue Implementation */
int infra_async_queue_init(struct infra_async_queue* queue) {
    infra_list_init(&queue->tasks);
    infra_mutex_init(&queue->lock);
    infra_cond_init(&queue->not_empty);
    queue->size = 0;
    queue->shutdown = 0;
    return INFRA_OK;
}

void infra_async_queue_destroy(struct infra_async_queue* queue) {
    struct infra_list* pos;
    struct infra_list* tmp;
    
    infra_mutex_lock(&queue->lock);
    queue->shutdown = 1;
    
    /* Free all pending tasks */
    for (pos = queue->tasks.next; pos != &queue->tasks; ) {
        tmp = pos;
        pos = pos->next;
        struct infra_async_task* task = (struct infra_async_task*)tmp;
        infra_list_del(&task->list);
        infra_free(task);
    }
    
    infra_mutex_unlock(&queue->lock);
    infra_mutex_destroy(&queue->lock);
    infra_cond_destroy(&queue->not_empty);
}

int infra_async_queue_push(struct infra_async_queue* queue, infra_async_fn fn, void* arg) {
    struct infra_async_task* task;
    
    if (queue->shutdown) {
        return INFRA_ERR_BUSY;
    }
    
    task = infra_malloc(sizeof(*task));
    if (!task) {
        return INFRA_ERR_NOMEM;
    }
    
    task->fn = fn;
    task->arg = arg;
    task->status = 0;
    
    infra_mutex_lock(&queue->lock);
    infra_list_add(&queue->tasks, &task->list);
    queue->size++;
    infra_cond_signal(&queue->not_empty);
    infra_mutex_unlock(&queue->lock);
    
    return INFRA_OK;
}

int infra_async_queue_pop(struct infra_async_queue* queue, struct infra_async_task** task) {
    infra_mutex_lock(&queue->lock);
    
    while (infra_list_empty(&queue->tasks) && !queue->shutdown) {
        infra_cond_wait(&queue->not_empty, &queue->lock);
    }
    
    if (queue->shutdown) {
        infra_mutex_unlock(&queue->lock);
        return INFRA_ERR_BUSY;
    }
    
    *task = (struct infra_async_task*)queue->tasks.next;
    infra_list_del(&(*task)->list);
    queue->size--;
    
    infra_mutex_unlock(&queue->lock);
    return INFRA_OK;
}

void infra_async_queue_shutdown(struct infra_async_queue* queue) {
    infra_mutex_lock(&queue->lock);
    queue->shutdown = 1;
    infra_cond_broadcast(&queue->not_empty);
    infra_mutex_unlock(&queue->lock);
}

/* Worker Thread Implementation */
static void* worker_thread(void* arg) {
    struct infra_async_worker* worker = (struct infra_async_worker*)arg;
    struct infra_async_task* task;

    while (worker->running) {
        if (infra_async_queue_pop(worker->queue, &task) == 0) {
            if (task) {
                task->fn(task->arg);
                infra_free(task);
            }
        }
    }

    return NULL;
}

/* Thread Pool Implementation */
int infra_async_pool_init(struct infra_async_pool* pool, size_t num_workers) {
    int ret;
    size_t i;
    struct infra_async_worker* worker;
    pthread_t thread;

    if (!pool || num_workers == 0) {
        return -1;
    }

    ret = infra_async_queue_init(&pool->queue);
    if (ret != 0) {
        return ret;
    }

    infra_list_init(&pool->workers);
    pool->num_workers = num_workers;

    for (i = 0; i < num_workers; i++) {
        worker = infra_malloc(sizeof(struct infra_async_worker));
        if (!worker) {
            infra_async_pool_destroy(pool);
            return -1;
        }

        worker->queue = &pool->queue;
        worker->running = 1;
        
        ret = pthread_create(&thread, NULL, worker_thread, worker);
        if (ret != 0) {
            infra_free(worker);
            infra_async_pool_destroy(pool);
            return -1;
        }
        worker->thread = (void*)thread;

        infra_list_add(&pool->workers, &worker->list);
    }

    return 0;
}

void infra_async_pool_destroy(struct infra_async_pool* pool) {
    struct infra_list* pos;
    struct infra_list* n;
    struct infra_async_worker* worker;

    if (!pool) {
        return;
    }

    infra_async_queue_shutdown(&pool->queue);

    infra_list_for_each_safe(pos, n, &pool->workers) {
        worker = container_of(pos, struct infra_async_worker, list);
        worker->running = 0;
        pthread_join((pthread_t)worker->thread, NULL);
        infra_list_del(&worker->list);
        infra_free(worker);
    }

    infra_async_queue_destroy(&pool->queue);
}

int infra_async_pool_submit(struct infra_async_pool* pool, infra_async_fn fn, void* arg) {
    if (!pool || !fn) {
        return -1;
    }

    return infra_async_queue_push(&pool->queue, fn, arg);
}
