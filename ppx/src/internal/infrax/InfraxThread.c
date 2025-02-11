/**
 * @file InfraxThread.c
 * @brief Implementation of thread management functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "internal/infrax/InfraxThread.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxSync.h"
#include "internal/infrax/InfraxCore.h"

// Forward declarations of instance methods
static InfraxError thread_start(InfraxThread* self, InfraxThreadFunc func, void* arg);
static InfraxError thread_join(InfraxThread* self, void** result);
static InfraxThreadId thread_tid(InfraxThread* self);

// Forward declarations of thread pool methods
InfraxError infrax_thread_pool_create(InfraxThread* self, InfraxThreadPoolConfig* config);
InfraxError infrax_thread_pool_destroy(InfraxThread* self);
InfraxError infrax_thread_pool_submit(InfraxThread* self, InfraxThreadFunc func, void* arg);
InfraxError infrax_thread_pool_get_stats(InfraxThread* self, InfraxThreadPoolStats* stats);

// InfraxMemory* get_memory_manager(void);

// Thread function wrapper
static void* thread_func(void* arg) {
    InfraxThread* self = (InfraxThread*)arg;
    if (!self || !self->config.func) return NULL;
    
    self->result = self->config.func(self->config.arg);
    return self->result;
}

// Get or create memory manager
InfraxMemory* get_memory_manager(void) {
    static InfraxMemory* memory = NULL;
    if (!memory) {
        InfraxMemoryConfig config = {
            .initial_size = 1024 * 1024,  // 1MB
            .use_gc = false,
            .use_pool = true,
            .gc_threshold = 0
        };
        memory = InfraxMemoryClass.new(&config);
    }
    return memory;
}

// Constructor implementation
static InfraxThread* thread_new(InfraxThreadConfig* config) {
    if (!config || !config->name) {
        return NULL;
    }

    // Get memory manager
    InfraxMemory* memory = get_memory_manager();
    if (!memory) {
        return NULL;
    }

    // Allocate thread object
    InfraxThread* self = (InfraxThread*)memory->alloc(memory, sizeof(InfraxThread));
    if (!self) {
        return NULL;
    }

    // Initialize thread object
    memset(self, 0, sizeof(InfraxThread));
    
    // Copy configuration
    size_t name_len = strlen(config->name) + 1;
    char* name_copy = (char*)memory->alloc(memory, name_len);
    if (!name_copy) {
        memory->dealloc(memory, self);
        return NULL;
    }
    memcpy(name_copy, config->name, name_len);
    
    self->config = *config;
    self->config.name = name_copy;
    
    // Create state protection mutex
    self->mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    if (!self->mutex) {
        memory->dealloc(memory, name_copy);
        memory->dealloc(memory, self);
        return NULL;
    }
    
    // Set instance methods
    self->start = thread_start;
    self->join = thread_join;
    self->tid = thread_tid;
    
    return self;
}

// Destructor implementation
static void thread_free(InfraxThread* self) {
    if (!self) return;
    
    InfraxMemory* memory = get_memory_manager();
    if (!memory) return;
    
    if (self->is_running) {
        pthread_t handle = (pthread_t)self->native_handle;
        pthread_cancel(handle);
        pthread_join(handle, NULL);
    }
    
    if (self->mutex) {
        InfraxSyncClass.free(self->mutex);
    }
    
    if (self->config.name) {
        memory->dealloc(memory, (void*)self->config.name);
    }
    
    memory->dealloc(memory, self);
}

// The "static" interface implementation
const InfraxThreadClassType InfraxThreadClass = {
    .new = thread_new,
    .free = thread_free,
    .start = thread_start,
    .join = thread_join,
    .tid = thread_tid,
    .pool_create = infrax_thread_pool_create,
    .pool_destroy = infrax_thread_pool_destroy,
    .pool_submit = infrax_thread_pool_submit,
    .pool_get_stats = infrax_thread_pool_get_stats
};

// Instance methods implementation
static InfraxError thread_start(InfraxThread* self, InfraxThreadFunc func, void* arg) {
    if (!self) {
        return make_error(INFRAX_ERROR_THREAD_INVALID_ARGUMENT, "Invalid thread");
    }
    
    if (self->is_running) {
        return make_error(INFRAX_ERROR_THREAD_ALREADY_RUNNING, "Thread already running");
    }
    
    // If new function and arguments are provided, update them
    if (func) {
        self->config.func = func;
        self->config.arg = arg;
    } else if (!self->config.func) {
        return make_error(INFRAX_ERROR_THREAD_INVALID_ARGUMENT, "No thread function specified");
    }
    
    // Set thread attributes
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to initialize thread attributes");
    }
    
    // Set stack size if specified
    if (self->config.stack_size > 0) {
        if (pthread_attr_setstacksize(&attr, self->config.stack_size) != 0) {
            pthread_attr_destroy(&attr);
            return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to set thread stack size");
        }
    }
    
    // Create thread
    pthread_t handle;
    int rc = pthread_create(&handle, &attr, thread_func, self);
    pthread_attr_destroy(&attr);
    
    if (rc != 0) {
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to create thread");
    }
    
    self->native_handle = (void*)handle;
    self->is_running = true;
    
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError thread_join(InfraxThread* self, void** result) {
    if (!self) {
        return make_error(INFRAX_ERROR_THREAD_INVALID_ARGUMENT, "Invalid thread");
    }
    
    if (!self->is_running) {
        return make_error(INFRAX_ERROR_THREAD_NOT_RUNNING, "Thread not running");
    }
    
    pthread_t handle = (pthread_t)self->native_handle;
    int rc = pthread_join(handle, result);
    
    if (rc != 0) {
        return make_error(INFRAX_ERROR_THREAD_JOIN_FAILED, "Failed to join thread");
    }
    
    self->is_running = false;
    
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxThreadId thread_tid(InfraxThread* self) {
    if (!self || !self->native_handle) return 0;
    return (InfraxThreadId)self->native_handle;
}

// 任务结构体
typedef struct {
    InfraxThreadFunc func;
    void* arg;
} ThreadTask;

// 线程池结构体
typedef struct {
    InfraxThreadPoolConfig config;
    InfraxThreadPoolStats stats;
    
    ThreadTask* task_queue;
    int queue_head;
    int queue_tail;
    int queue_size;
    
    InfraxThread** workers;
    int num_workers;
    
    InfraxSync* mutex;
    InfraxSync* not_empty;
    InfraxSync* not_full;
    
    bool shutdown;
} ThreadPool;

// 工作线程函数
static void* worker_thread(void* arg) {
    InfraxThread* self = (InfraxThread*)arg;
    ThreadPool* pool = (ThreadPool*)self->private_data;
    
    while (true) {
        pool->mutex->mutex_lock(pool->mutex);
        
        while (pool->queue_head == pool->queue_tail && !pool->shutdown) {
            pool->not_empty->cond_wait(pool->not_empty, pool->mutex);
        }
        
        if (pool->shutdown && pool->queue_head == pool->queue_tail) {
            pool->mutex->mutex_unlock(pool->mutex);
            break;
        }
        
        // 获取任务
        ThreadTask task = pool->task_queue[pool->queue_head];
        pool->queue_head = (pool->queue_head + 1) % pool->queue_size;
        
        pool->stats.active_threads++;
        pool->stats.idle_threads--;
        pool->stats.pending_tasks--;
        
        pool->mutex->mutex_unlock(pool->mutex);
        pool->not_full->cond_signal(pool->not_full);
        
        // 执行任务
        task.func(task.arg);
        
        pool->mutex->mutex_lock(pool->mutex);
        pool->stats.active_threads--;
        pool->stats.idle_threads++;
        pool->stats.completed_tasks++;
        pool->mutex->mutex_unlock(pool->mutex);
    }
    
    return NULL;
}

// 线程池相关实现
InfraxError infrax_thread_pool_create(InfraxThread* self, InfraxThreadPoolConfig* config) {
    if (!self || !config) {
        return make_error(INFRAX_ERROR_THREAD_INVALID_ARGUMENT, "Invalid thread or config");
    }
    
    InfraxMemory* memory = get_memory_manager();
    if (!memory) {
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to get memory manager");
    }
    
    ThreadPool* pool = memory->alloc(memory, sizeof(ThreadPool));
    if (!pool) {
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to allocate thread pool");
    }
    
    pool->config = *config;
    pool->queue_head = 0;
    pool->queue_tail = 0;
    pool->shutdown = false;
    
    // 初始化任务队列
    pool->queue_size = config->queue_size;
    pool->task_queue = memory->alloc(memory, sizeof(ThreadTask) * config->queue_size);
    if (!pool->task_queue) {
        memory->dealloc(memory, pool);
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to allocate task queue");
    }
    
    // 初始化同步原语
    pool->mutex = InfraxSyncClass.new(INFRAX_SYNC_TYPE_MUTEX);
    pool->not_empty = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    pool->not_full = InfraxSyncClass.new(INFRAX_SYNC_TYPE_CONDITION);
    
    if (!pool->mutex || !pool->not_empty || !pool->not_full) {
        if (pool->mutex) InfraxSyncClass.free(pool->mutex);
        if (pool->not_empty) InfraxSyncClass.free(pool->not_empty);
        if (pool->not_full) InfraxSyncClass.free(pool->not_full);
        memory->dealloc(memory, pool->task_queue);
        memory->dealloc(memory, pool);
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to create synchronization primitives");
    }
    
    // 创建工作线程
    pool->workers = memory->alloc(memory, sizeof(InfraxThread*) * config->min_threads);
    if (!pool->workers) {
        InfraxSyncClass.free(pool->mutex);
        InfraxSyncClass.free(pool->not_empty);
        InfraxSyncClass.free(pool->not_full);
        memory->dealloc(memory, pool->task_queue);
        memory->dealloc(memory, pool);
        return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to allocate worker array");
    }
    
    for (int i = 0; i < config->min_threads; i++) {
        char* worker_name = memory->alloc(memory, 32);
        if (!worker_name) {
            for (int j = 0; j < i; j++) {
                InfraxThreadClass.free(pool->workers[j]);
            }
            memory->dealloc(memory, pool->workers);
            InfraxSyncClass.free(pool->mutex);
            InfraxSyncClass.free(pool->not_empty);
            InfraxSyncClass.free(pool->not_full);
            memory->dealloc(memory, pool->task_queue);
            memory->dealloc(memory, pool);
            return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to allocate worker name");
        }
        
        snprintf(worker_name, 32, "worker_%d", i);
        InfraxThreadConfig worker_config = {
            .name = worker_name,
            .func = worker_thread,
            .arg = NULL,
            .stack_size = 0,
            .priority = 0
        };
        
        pool->workers[i] = InfraxThreadClass.new(&worker_config);
        if (!pool->workers[i]) {
            memory->dealloc(memory, worker_name);
            for (int j = 0; j < i; j++) {
                InfraxThreadClass.free(pool->workers[j]);
            }
            memory->dealloc(memory, pool->workers);
            InfraxSyncClass.free(pool->mutex);
            InfraxSyncClass.free(pool->not_empty);
            InfraxSyncClass.free(pool->not_full);
            memory->dealloc(memory, pool->task_queue);
            memory->dealloc(memory, pool);
            return make_error(INFRAX_ERROR_THREAD_CREATE_FAILED, "Failed to create worker thread");
        }
        
        pool->workers[i]->private_data = pool;
        InfraxError err = pool->workers[i]->start(pool->workers[i], worker_thread, pool->workers[i]);
        if (INFRAX_ERROR_IS_ERR(err)) {
            memory->dealloc(memory, worker_name);
            for (int j = 0; j <= i; j++) {
                InfraxThreadClass.free(pool->workers[j]);
            }
            memory->dealloc(memory, pool->workers);
            InfraxSyncClass.free(pool->mutex);
            InfraxSyncClass.free(pool->not_empty);
            InfraxSyncClass.free(pool->not_full);
            memory->dealloc(memory, pool->task_queue);
            memory->dealloc(memory, pool);
            return err;
        }
    }
    
    pool->num_workers = config->min_threads;
    pool->stats.idle_threads = config->min_threads;
    pool->stats.active_threads = 0;
    pool->stats.pending_tasks = 0;
    pool->stats.completed_tasks = 0;
    
    self->private_data = pool;
    return INFRAX_ERROR_OK_STRUCT;
}

InfraxError infrax_thread_pool_destroy(InfraxThread* self) {
    if (!self || !self->private_data) {
        return make_error(INFRAX_ERROR_THREAD_INVALID_ARGUMENT, "Invalid thread or pool not initialized");
    }
    
    ThreadPool* pool = (ThreadPool*)self->private_data;
    InfraxMemory* memory = get_memory_manager();
    if (!memory) {
        return make_error(INFRAX_ERROR_THREAD_JOIN_FAILED, "Failed to get memory manager");
    }
    
    // 设置关闭标志
    pool->mutex->mutex_lock(pool->mutex);
    pool->shutdown = true;
    pool->mutex->mutex_unlock(pool->mutex);
    
    // 唤醒所有等待的线程
    pool->not_empty->cond_broadcast(pool->not_empty);
    
    // 等待所有工作线程结束
    for (int i = 0; i < pool->num_workers; i++) {
        void* result;
        InfraxError err = pool->workers[i]->join(pool->workers[i], &result);
        if (INFRAX_ERROR_IS_ERR(err)) {
            // 继续清理，但返回第一个错误
            continue;
        }
        InfraxThreadClass.free(pool->workers[i]);
    }
    
    memory->dealloc(memory, pool->workers);
    InfraxSyncClass.free(pool->mutex);
    InfraxSyncClass.free(pool->not_empty);
    InfraxSyncClass.free(pool->not_full);
    memory->dealloc(memory, pool->task_queue);
    memory->dealloc(memory, pool);
    
    self->private_data = NULL;
    return INFRAX_ERROR_OK_STRUCT;
}

InfraxError infrax_thread_pool_submit(InfraxThread* self, InfraxThreadFunc func, void* arg) {
    if (!self || !self->private_data || !func) {
        return make_error(INFRAX_ERROR_THREAD_INVALID_ARGUMENT, "Invalid thread, pool not initialized, or null function");
    }
    
    ThreadPool* pool = (ThreadPool*)self->private_data;
    
    pool->mutex->mutex_lock(pool->mutex);
    
    while (((pool->queue_tail + 1) % pool->queue_size) == pool->queue_head) {
        if (pool->shutdown) {
            pool->mutex->mutex_unlock(pool->mutex);
            return make_error(INFRAX_ERROR_THREAD_NOT_RUNNING, "Thread pool is shutting down");
        }
        pool->not_full->cond_wait(pool->not_full, pool->mutex);
    }
    
    if (pool->shutdown) {
        pool->mutex->mutex_unlock(pool->mutex);
        return make_error(INFRAX_ERROR_THREAD_NOT_RUNNING, "Thread pool is shutting down");
    }
    
    // 添加任务到队列
    pool->task_queue[pool->queue_tail].func = func;
    pool->task_queue[pool->queue_tail].arg = arg;
    pool->queue_tail = (pool->queue_tail + 1) % pool->queue_size;
    pool->stats.pending_tasks++;
    
    pool->mutex->mutex_unlock(pool->mutex);
    pool->not_empty->cond_signal(pool->not_empty);
    
    return INFRAX_ERROR_OK_STRUCT;
}

InfraxError infrax_thread_pool_get_stats(InfraxThread* self, InfraxThreadPoolStats* stats) {
    if (!self || !self->private_data || !stats) {
        return make_error(INFRAX_ERROR_THREAD_INVALID_ARGUMENT, "Invalid thread, pool not initialized, or null stats");
    }
    
    ThreadPool* pool = (ThreadPool*)self->private_data;
    
    pool->mutex->mutex_lock(pool->mutex);
    *stats = pool->stats;
    pool->mutex->mutex_unlock(pool->mutex);
    
    return INFRAX_ERROR_OK_STRUCT;
}
