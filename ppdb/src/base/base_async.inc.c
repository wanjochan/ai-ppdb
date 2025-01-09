/*
 * base_async.inc.c - Asynchronous System Implementation
 *
 * This file contains:
 * 1. IO Manager
 *    - Priority-based IO request scheduling
 *    - Dynamic thread pool management
 *    - Performance statistics
 *
 * 2. Event System
 *    - Cross-platform event handling (IOCP/epoll)
 *    - Event filtering mechanism
 *    - Unified event interface
 *
 * 3. Timer System
 *    - High-precision timer implementation
 *    - Timer wheel algorithm
 *    - Priority-based scheduling
 */

#include <cosmopolitan.h>
#include "internal/base.h"

//-----------------------------------------------------------------------------
// Common Definitions
//-----------------------------------------------------------------------------

// Default values for IO manager
#define PPDB_IO_DEFAULT_QUEUE_SIZE 1024
#define PPDB_IO_MIN_THREADS 2
#define PPDB_IO_MAX_THREADS 64
#define PPDB_IO_DEFAULT_THREADS 4
#define PPDB_IO_QUEUE_PRIORITIES 4

// Event system constants
#define PPDB_EVENT_MAX_EVENTS 64
#define PPDB_EVENT_MAX_FILTERS 16

// Timer wheel configuration
#define PPDB_TIMER_WHEEL_BITS 8
#define PPDB_TIMER_WHEEL_SIZE (1 << PPDB_TIMER_WHEEL_BITS)
#define PPDB_TIMER_WHEEL_MASK (PPDB_TIMER_WHEEL_SIZE - 1)
#define PPDB_TIMER_WHEEL_COUNT 4

// Timer priorities
#define PPDB_TIMER_PRIORITY_HIGH 0
#define PPDB_TIMER_PRIORITY_NORMAL 1
#define PPDB_TIMER_PRIORITY_LOW 2
#define PPDB_TIMER_PRIORITY_IDLE 3
#define PPDB_TIMER_PRIORITY_COUNT 4

// Timer flags
#define PPDB_TIMER_FLAG_NONE 0x00
#define PPDB_TIMER_FLAG_REPEAT 0x01
#define PPDB_TIMER_FLAG_PRECISE 0x02
#define PPDB_TIMER_FLAG_COALESCE 0x04

//-----------------------------------------------------------------------------
// IO Manager Implementation
//-----------------------------------------------------------------------------

// IO request structure
typedef struct ppdb_base_io_request {
    ppdb_base_io_func_t func;     // IO function
    void* arg;                    // Function argument
    int priority;                 // Request priority (0-3, 0 is highest)
    uint64_t timestamp;           // Request timestamp
    struct ppdb_base_io_request* next;  // Next request in queue
} ppdb_base_io_request_t;

// Priority queue structure
typedef struct ppdb_base_io_queue {
    ppdb_base_io_request_t* head;  // Queue head
    ppdb_base_io_request_t* tail;  // Queue tail
    size_t size;                   // Current queue size
    uint64_t total_requests;       // Total requests processed
    uint64_t total_wait_time;      // Total wait time in nanoseconds
} ppdb_base_io_queue_t;

// Thread pool worker
typedef struct ppdb_base_io_worker {
    ppdb_base_thread_t* thread;    // Worker thread
    bool active;                   // Worker is active
    int cpu_id;                    // Preferred CPU ID
    uint64_t processed_requests;   // Number of requests processed
    uint64_t total_work_time;      // Total work time in nanoseconds
    struct ppdb_base_io_worker* next;  // Next worker in list
} ppdb_base_io_worker_t;

// IO manager structure
struct ppdb_base_io_manager {
    ppdb_base_mutex_t* mutex;      // Manager mutex
    ppdb_base_cond_t* cond;        // Condition variable
    bool running;                  // Manager is running
    size_t max_queue_size;         // Maximum queue size
    
    // Priority queues
    ppdb_base_io_queue_t queues[PPDB_IO_QUEUE_PRIORITIES];
    
    // Thread pool
    ppdb_base_io_worker_t* workers;  // Worker list
    size_t min_threads;             // Minimum number of threads
    size_t max_threads;             // Maximum number of threads
    size_t active_threads;          // Current number of active threads
    
    // Statistics
    struct {
        uint64_t total_requests;     // Total requests received
        uint64_t rejected_requests;  // Requests rejected due to queue full
        uint64_t total_wait_time;    // Total request wait time
        uint64_t total_work_time;    // Total request processing time
    } stats;
};

// Initialize priority queue
static void init_queue(ppdb_base_io_queue_t* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->total_requests = 0;
    queue->total_wait_time = 0;
}

// Add request to priority queue
static ppdb_error_t queue_push(ppdb_base_io_queue_t* queue, ppdb_base_io_request_t* req) {
    if (!queue->head) {
        queue->head = queue->tail = req;
    } else {
        queue->tail->next = req;
        queue->tail = req;
    }
    queue->size++;
    return PPDB_OK;
}

// Get request from priority queue
static ppdb_base_io_request_t* queue_pop(ppdb_base_io_queue_t* queue) {
    if (!queue->head) return NULL;
    
    ppdb_base_io_request_t* req = queue->head;
    queue->head = req->next;
    if (!queue->head) queue->tail = NULL;
    queue->size--;
    req->next = NULL;
    
    uint64_t wait_time = ppdb_base_get_time_ns() - req->timestamp;
    queue->total_wait_time += wait_time;
    queue->total_requests++;
    
    return req;
}

// Worker thread function
static void* worker_thread(void* arg) {
    ppdb_base_io_worker_t* worker = (ppdb_base_io_worker_t*)arg;
    ppdb_base_io_manager_t* mgr = (ppdb_base_io_manager_t*)worker->thread->user_data;

    // Set CPU affinity if specified
    if (worker->cpu_id >= 0) {
        ppdb_base_set_thread_affinity(worker->thread, worker->cpu_id);
    }

    while (worker->active) {
        ppdb_base_io_request_t* req = NULL;
        
        // Lock manager
        ppdb_base_mutex_lock(mgr->mutex);
        
        // Check all priority queues
        while (!req && worker->active) {
            for (int i = 0; i < PPDB_IO_QUEUE_PRIORITIES; i++) {
                if (mgr->queues[i].head) {
                    req = queue_pop(&mgr->queues[i]);
                    break;
                }
            }
            
            if (!req) {
                // No work available, wait for signal
                ppdb_base_cond_wait(mgr->cond, mgr->mutex);
            }
        }
        
        ppdb_base_mutex_unlock(mgr->mutex);
        
        // Process request
        if (req && req->func) {
            uint64_t start_time = ppdb_base_get_time_ns();
            req->func(req->arg);
            uint64_t work_time = ppdb_base_get_time_ns() - start_time;
            
            // Update statistics
            worker->processed_requests++;
            worker->total_work_time += work_time;
            
            ppdb_base_free(req);
        }
    }

    return NULL;
}

// Create worker thread
static ppdb_error_t create_worker(ppdb_base_io_manager_t* mgr, int cpu_id) {
    ppdb_base_io_worker_t* worker = ppdb_base_malloc(sizeof(ppdb_base_io_worker_t));
    if (!worker) return PPDB_BASE_ERR_MEMORY;
    
    worker->active = true;
    worker->cpu_id = cpu_id;
    worker->processed_requests = 0;
    worker->total_work_time = 0;
    worker->next = NULL;
    
    ppdb_error_t err = ppdb_base_thread_create(&worker->thread, worker_thread, worker);
    if (err != PPDB_OK) {
        ppdb_base_free(worker);
        return err;
    }
    
    worker->thread->user_data = mgr;
    
    // Add to worker list
    worker->next = mgr->workers;
    mgr->workers = worker;
    mgr->active_threads++;
    
    return PPDB_OK;
}

// Destroy worker thread
static void destroy_worker(ppdb_base_io_worker_t* worker) {
    if (!worker) return;
    
    worker->active = false;
    if (worker->thread) {
        ppdb_base_thread_join(worker->thread);
        ppdb_base_thread_destroy(worker->thread);
    }
    ppdb_base_free(worker);
}

// Create IO manager
ppdb_error_t ppdb_base_io_manager_create(ppdb_base_io_manager_t** mgr, size_t queue_size, size_t num_threads) {
    if (!mgr) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_io_manager_t* new_mgr = ppdb_base_malloc(sizeof(ppdb_base_io_manager_t));
    if (!new_mgr) return PPDB_BASE_ERR_MEMORY;
    
    memset(new_mgr, 0, sizeof(ppdb_base_io_manager_t));
    
    // Initialize queues
    new_mgr->max_queue_size = queue_size > 0 ? queue_size : PPDB_IO_DEFAULT_QUEUE_SIZE;
    for (int i = 0; i < PPDB_IO_QUEUE_PRIORITIES; i++) {
        init_queue(&new_mgr->queues[i]);
    }
    
    // Create mutex and condition variable
    ppdb_error_t err = ppdb_base_mutex_create(&new_mgr->mutex);
    if (err != PPDB_OK) {
        ppdb_base_free(new_mgr);
        return err;
    }
    
    err = ppdb_base_cond_create(&new_mgr->cond);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(new_mgr->mutex);
        ppdb_base_free(new_mgr);
        return err;
    }
    
    // Set thread pool parameters
    new_mgr->min_threads = PPDB_IO_MIN_THREADS;
    new_mgr->max_threads = PPDB_IO_MAX_THREADS;
    size_t initial_threads = num_threads > 0 ? num_threads : PPDB_IO_DEFAULT_THREADS;
    if (initial_threads < new_mgr->min_threads) initial_threads = new_mgr->min_threads;
    if (initial_threads > new_mgr->max_threads) initial_threads = new_mgr->max_threads;
    
    // Create initial worker threads
    new_mgr->running = true;
    for (size_t i = 0; i < initial_threads; i++) {
        err = create_worker(new_mgr, i % ppdb_base_get_cpu_count());
        if (err != PPDB_OK) {
            ppdb_base_io_manager_destroy(new_mgr);
            return err;
        }
    }
    
    *mgr = new_mgr;
    return PPDB_OK;
}

// Destroy IO manager
ppdb_error_t ppdb_base_io_manager_destroy(ppdb_base_io_manager_t* mgr) {
    if (!mgr) return PPDB_BASE_ERR_PARAM;
    
    // Stop all workers
    mgr->running = false;
    ppdb_base_mutex_lock(mgr->mutex);
    ppdb_base_cond_broadcast(mgr->cond);
    ppdb_base_mutex_unlock(mgr->mutex);
    
    // Wait for workers to finish
    while (mgr->workers) {
        ppdb_base_io_worker_t* worker = mgr->workers;
        mgr->workers = worker->next;
        destroy_worker(worker);
        mgr->active_threads--;
    }
    
    // Free pending requests
    for (int i = 0; i < PPDB_IO_QUEUE_PRIORITIES; i++) {
        while (mgr->queues[i].head) {
            ppdb_base_io_request_t* req = queue_pop(&mgr->queues[i]);
            ppdb_base_free(req);
        }
    }
    
    // Cleanup synchronization objects
    ppdb_base_cond_destroy(mgr->cond);
    ppdb_base_mutex_destroy(mgr->mutex);
    ppdb_base_free(mgr);
    
    return PPDB_OK;
}

// Schedule IO request with priority
ppdb_error_t ppdb_base_io_manager_schedule_priority(ppdb_base_io_manager_t* mgr,
                                                  ppdb_base_io_func_t func,
                                                  void* arg,
                                                  int priority) {
    if (!mgr || !func || priority < 0 || priority >= PPDB_IO_QUEUE_PRIORITIES) {
        return PPDB_BASE_ERR_PARAM;
    }
    
    ppdb_base_mutex_lock(mgr->mutex);
    
    // Check queue size limit
    size_t total_size = 0;
    for (int i = 0; i < PPDB_IO_QUEUE_PRIORITIES; i++) {
        total_size += mgr->queues[i].size;
    }
    
    if (total_size >= mgr->max_queue_size) {
        mgr->stats.rejected_requests++;
        ppdb_base_mutex_unlock(mgr->mutex);
        return PPDB_BASE_ERR_FULL;
    }
    
    // Create request
    ppdb_base_io_request_t* req = ppdb_base_malloc(sizeof(ppdb_base_io_request_t));
    if (!req) {
        ppdb_base_mutex_unlock(mgr->mutex);
        return PPDB_BASE_ERR_MEMORY;
    }
    
    req->func = func;
    req->arg = arg;
    req->priority = priority;
    req->timestamp = ppdb_base_get_time_ns();
    req->next = NULL;
    
    // Add to queue
    ppdb_error_t err = queue_push(&mgr->queues[priority], req);
    if (err != PPDB_OK) {
        ppdb_base_free(req);
        ppdb_base_mutex_unlock(mgr->mutex);
        return err;
    }
    
    mgr->stats.total_requests++;
    
    // Signal workers
    ppdb_base_cond_signal(mgr->cond);
    
    ppdb_base_mutex_unlock(mgr->mutex);
    return PPDB_OK;
}

// Schedule IO request (default priority)
ppdb_error_t ppdb_base_io_manager_schedule(ppdb_base_io_manager_t* mgr,
                                         ppdb_base_io_func_t func,
                                         void* arg) {
    return ppdb_base_io_manager_schedule_priority(mgr, func, arg, PPDB_IO_QUEUE_PRIORITIES / 2);
}

// Get IO manager statistics
void ppdb_base_io_manager_get_stats(ppdb_base_io_manager_t* mgr,
                                  uint64_t* total_requests,
                                  uint64_t* rejected_requests,
                                  uint64_t* total_wait_time,
                                  uint64_t* total_work_time) {
    if (!mgr) return;
    
    ppdb_base_mutex_lock(mgr->mutex);
    
    if (total_requests) *total_requests = mgr->stats.total_requests;
    if (rejected_requests) *rejected_requests = mgr->stats.rejected_requests;
    if (total_wait_time) *total_wait_time = mgr->stats.total_wait_time;
    if (total_work_time) *total_work_time = mgr->stats.total_work_time;
    
    ppdb_base_mutex_unlock(mgr->mutex);
}

//-----------------------------------------------------------------------------
// Event System Implementation
//-----------------------------------------------------------------------------

// Event filter function type
typedef bool (*ppdb_base_event_filter_func)(ppdb_base_event_handler_t* handler, uint32_t events);

// Event filter structure
typedef struct ppdb_base_event_filter {
    ppdb_base_event_filter_func func;  // Filter function
    void* user_data;                   // User data
    struct ppdb_base_event_filter* next;  // Next filter in chain
} ppdb_base_event_filter_t;

// Platform-specific implementation interface
typedef struct ppdb_base_event_impl_ops {
    const char* name;
    ppdb_error_t (*init)(void** context);
    void (*cleanup)(void* context);
    ppdb_error_t (*add)(void* context, ppdb_base_event_handler_t* handler);
    ppdb_error_t (*remove)(void* context, ppdb_base_event_handler_t* handler);
    ppdb_error_t (*modify)(void* context, ppdb_base_event_handler_t* handler);
    ppdb_error_t (*wait)(void* context, int timeout_ms);
} ppdb_base_event_impl_ops_t;

// Event loop structure
struct ppdb_base_event_loop {
    ppdb_base_mutex_t* mutex;          // Protects event loop state
    bool running;                      // Is event loop running
    ppdb_base_event_handler_t* handlers;  // List of event handlers
    ppdb_base_timer_t* timers;         // List of timers
    ppdb_base_event_filter_t* filters; // List of event filters
    void* impl;                        // Implementation-specific data
    const ppdb_base_event_impl_ops_t* ops;  // Implementation operations
    struct {
        uint64_t total_events;         // Total events processed
        uint64_t total_errors;         // Total error events
        uint64_t total_timeouts;       // Total timer timeouts
        uint64_t total_wait_time_us;   // Total wait time in microseconds
        uint64_t filtered_events;      // Events filtered out
    } stats;
};

#if defined(__COSMOPOLITAN__)

//-----------------------------------------------------------------------------
// Windows IOCP Implementation
//-----------------------------------------------------------------------------

typedef struct iocp_context {
    HANDLE iocp;                     // IOCP handle
    OVERLAPPED_ENTRY events[PPDB_EVENT_MAX_EVENTS];  // Event buffer
} iocp_context_t;

static ppdb_error_t iocp_init(void** context) {
    iocp_context_t* ctx = ppdb_base_malloc(sizeof(iocp_context_t));
    if (!ctx) return PPDB_BASE_ERR_MEMORY;

    ctx->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!ctx->iocp) {
        ppdb_base_free(ctx);
        return PPDB_BASE_ERR_SYSTEM;
    }

    *context = ctx;
    return PPDB_OK;
}

static void iocp_cleanup(void* context) {
    iocp_context_t* ctx = (iocp_context_t*)context;
    if (ctx) {
        if (ctx->iocp) CloseHandle(ctx->iocp);
        ppdb_base_free(ctx);
    }
}

static ppdb_error_t iocp_add(void* context, ppdb_base_event_handler_t* handler) {
    iocp_context_t* ctx = (iocp_context_t*)context;
    
    HANDLE handle = CreateIoCompletionPort((HANDLE)handler->fd, ctx->iocp,
                                         (ULONG_PTR)handler, 0);
    if (handle != ctx->iocp) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}

static ppdb_error_t iocp_remove(void* context, ppdb_base_event_handler_t* handler) {
    // IOCP doesn't need explicit removal
    return PPDB_OK;
}

static ppdb_error_t iocp_modify(void* context, ppdb_base_event_handler_t* handler) {
    // IOCP doesn't need explicit modification
    return PPDB_OK;
}

static ppdb_error_t iocp_wait(void* context, int timeout_ms) {
    iocp_context_t* ctx = (iocp_context_t*)context;
    DWORD num_events = 0;
    
    BOOL success = GetQueuedCompletionStatusEx(ctx->iocp,
                                             ctx->events,
                                             PPDB_EVENT_MAX_EVENTS,
                                             &num_events,
                                             timeout_ms,
                                             FALSE);
    
    if (!success) {
        DWORD error = GetLastError();
        if (error == WAIT_TIMEOUT) return PPDB_OK;
        return PPDB_BASE_ERR_SYSTEM;
    }

    for (DWORD i = 0; i < num_events; i++) {
        ppdb_base_event_handler_t* handler = 
            (ppdb_base_event_handler_t*)ctx->events[i].lpCompletionKey;
        
        if (handler && handler->callback) {
            uint32_t events = PPDB_EVENT_NONE;
            OVERLAPPED_ENTRY* entry = &ctx->events[i];
            
            if (entry->dwNumberOfBytesTransferred > 0) {
                events |= PPDB_EVENT_READ | PPDB_EVENT_WRITE;
            }
            if (entry->Internal != 0) {
                events |= PPDB_EVENT_ERROR;
            }
            
            handler->callback(handler, events);
        }
    }

    return PPDB_OK;
}

static const ppdb_base_event_impl_ops_t iocp_ops = {
    .name = "iocp",
    .init = iocp_init,
    .cleanup = iocp_cleanup,
    .add = iocp_add,
    .remove = iocp_remove,
    .modify = iocp_modify,
    .wait = iocp_wait
};

//-----------------------------------------------------------------------------
// Linux epoll Implementation
//-----------------------------------------------------------------------------

typedef struct epoll_context {
    int epoll_fd;                   // epoll file descriptor
    struct epoll_event events[PPDB_EVENT_MAX_EVENTS];  // Event buffer
} epoll_context_t;

static ppdb_error_t epoll_init(void** context) {
    epoll_context_t* ctx = ppdb_base_malloc(sizeof(epoll_context_t));
    if (!ctx) return PPDB_BASE_ERR_MEMORY;

    ctx->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx->epoll_fd < 0) {
        ppdb_base_free(ctx);
        return PPDB_BASE_ERR_SYSTEM;
    }

    *context = ctx;
    return PPDB_OK;
}

static void epoll_cleanup(void* context) {
    epoll_context_t* ctx = (epoll_context_t*)context;
    if (ctx) {
        if (ctx->epoll_fd >= 0) close(ctx->epoll_fd);
        ppdb_base_free(ctx);
    }
}

static ppdb_error_t epoll_add(void* context, ppdb_base_event_handler_t* handler) {
    epoll_context_t* ctx = (epoll_context_t*)context;
    
    struct epoll_event ev;
    ev.events = 0;
    if (handler->events & PPDB_EVENT_READ)  ev.events |= EPOLLIN;
    if (handler->events & PPDB_EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.ptr = handler;

    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, handler->fd, &ev) < 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}

static ppdb_error_t epoll_remove(void* context, ppdb_base_event_handler_t* handler) {
    epoll_context_t* ctx = (epoll_context_t*)context;
    
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, handler->fd, NULL) < 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}

static ppdb_error_t epoll_modify(void* context, ppdb_base_event_handler_t* handler) {
    epoll_context_t* ctx = (epoll_context_t*)context;
    
    struct epoll_event ev;
    ev.events = 0;
    if (handler->events & PPDB_EVENT_READ)  ev.events |= EPOLLIN;
    if (handler->events & PPDB_EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.ptr = handler;

    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_MOD, handler->fd, &ev) < 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }

    return PPDB_OK;
}

static ppdb_error_t epoll_wait(void* context, int timeout_ms) {
    epoll_context_t* ctx = (epoll_context_t*)context;
    
    int nfds = epoll_wait(ctx->epoll_fd, ctx->events, PPDB_EVENT_MAX_EVENTS, timeout_ms);
    if (nfds < 0) {
        if (errno == EINTR) return PPDB_OK;
        return PPDB_BASE_ERR_SYSTEM;
    }

    for (int i = 0; i < nfds; i++) {
        ppdb_base_event_handler_t* handler = ctx->events[i].data.ptr;
        if (handler && handler->callback) {
            uint32_t events = PPDB_EVENT_NONE;
            
            if (ctx->events[i].events & EPOLLIN)  events |= PPDB_EVENT_READ;
            if (ctx->events[i].events & EPOLLOUT) events |= PPDB_EVENT_WRITE;
            if (ctx->events[i].events & EPOLLERR) events |= PPDB_EVENT_ERROR;
            
            handler->callback(handler, events);
        }
    }

    return PPDB_OK;
}

static const ppdb_base_event_impl_ops_t epoll_ops = {
    .name = "epoll",
    .init = epoll_init,
    .cleanup = epoll_cleanup,
    .add = epoll_add,
    .remove = epoll_remove,
    .modify = epoll_modify,
    .wait = epoll_wait
};

#endif // __COSMOPOLITAN__

// Create event loop
ppdb_error_t ppdb_base_event_loop_create(ppdb_base_event_loop_t** loop) {
    if (!loop) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_event_loop_t* new_loop = ppdb_base_malloc(sizeof(ppdb_base_event_loop_t));
    if (!new_loop) return PPDB_BASE_ERR_MEMORY;
    
    memset(new_loop, 0, sizeof(ppdb_base_event_loop_t));
    
    // Create mutex
    ppdb_error_t err = ppdb_base_mutex_create(&new_loop->mutex);
    if (err != PPDB_OK) {
        ppdb_base_free(new_loop);
        return err;
    }
    
    // Initialize platform-specific implementation
#if defined(__COSMOPOLITAN__)
    if (ppdb_base_is_windows()) {
        new_loop->ops = &iocp_ops;
    } else {
        new_loop->ops = &epoll_ops;
    }
#else
    new_loop->ops = &epoll_ops;
#endif
    
    err = new_loop->ops->init(&new_loop->impl);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(new_loop->mutex);
        ppdb_base_free(new_loop);
        return err;
    }
    
    new_loop->running = true;
    *loop = new_loop;
    return PPDB_OK;
}

// Destroy event loop
void ppdb_base_event_loop_destroy(ppdb_base_event_loop_t* loop) {
    if (!loop) return;
    
    loop->running = false;
    
    // Cleanup handlers
    while (loop->handlers) {
        ppdb_base_event_handler_t* handler = loop->handlers;
        loop->handlers = handler->next;
        ppdb_base_free(handler);
    }
    
    // Cleanup filters
    while (loop->filters) {
        ppdb_base_event_filter_t* filter = loop->filters;
        loop->filters = filter->next;
        ppdb_base_free(filter);
    }
    
    // Cleanup implementation
    if (loop->ops && loop->ops->cleanup) {
        loop->ops->cleanup(loop->impl);
    }
    
    ppdb_base_mutex_destroy(loop->mutex);
    ppdb_base_free(loop);
}

// Add event handler
ppdb_error_t ppdb_base_event_handler_add(ppdb_base_event_loop_t* loop,
                                       ppdb_base_event_handler_t* handler) {
    if (!loop || !handler) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_mutex_lock(loop->mutex);
    
    // Add to implementation
    ppdb_error_t err = loop->ops->add(loop->impl, handler);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(loop->mutex);
        return err;
    }
    
    // Add to handler list
    handler->next = loop->handlers;
    loop->handlers = handler;
    
    ppdb_base_mutex_unlock(loop->mutex);
    return PPDB_OK;
}

// Remove event handler
ppdb_error_t ppdb_base_event_handler_remove(ppdb_base_event_loop_t* loop,
                                          ppdb_base_event_handler_t* handler) {
    if (!loop || !handler) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_mutex_lock(loop->mutex);
    
    // Remove from implementation
    ppdb_error_t err = loop->ops->remove(loop->impl, handler);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(loop->mutex);
        return err;
    }
    
    // Remove from handler list
    ppdb_base_event_handler_t** curr = &loop->handlers;
    while (*curr) {
        if (*curr == handler) {
            *curr = handler->next;
            break;
        }
        curr = &(*curr)->next;
    }
    
    ppdb_base_mutex_unlock(loop->mutex);
    return PPDB_OK;
}

// Modify event handler
ppdb_error_t ppdb_base_event_handler_modify(ppdb_base_event_loop_t* loop,
                                          ppdb_base_event_handler_t* handler) {
    if (!loop || !handler) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_mutex_lock(loop->mutex);
    
    ppdb_error_t err = loop->ops->modify(loop->impl, handler);
    
    ppdb_base_mutex_unlock(loop->mutex);
    return err;
}

// Add event filter
ppdb_error_t ppdb_base_event_filter_add(ppdb_base_event_loop_t* loop,
                                      ppdb_base_event_filter_func filter,
                                      void* user_data) {
    if (!loop || !filter) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_event_filter_t* new_filter = ppdb_base_malloc(sizeof(ppdb_base_event_filter_t));
    if (!new_filter) return PPDB_BASE_ERR_MEMORY;
    
    new_filter->func = filter;
    new_filter->user_data = user_data;
    
    ppdb_base_mutex_lock(loop->mutex);
    
    new_filter->next = loop->filters;
    loop->filters = new_filter;
    
    ppdb_base_mutex_unlock(loop->mutex);
    return PPDB_OK;
}

// Remove event filter
ppdb_error_t ppdb_base_event_filter_remove(ppdb_base_event_loop_t* loop,
                                         ppdb_base_event_filter_func filter) {
    if (!loop || !filter) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_mutex_lock(loop->mutex);
    
    ppdb_base_event_filter_t** curr = &loop->filters;
    while (*curr) {
        if ((*curr)->func == filter) {
            ppdb_base_event_filter_t* to_remove = *curr;
            *curr = to_remove->next;
            ppdb_base_free(to_remove);
            break;
        }
        curr = &(*curr)->next;
    }
    
    ppdb_base_mutex_unlock(loop->mutex);
    return PPDB_OK;
}

// Run event loop
ppdb_error_t ppdb_base_event_loop_run(ppdb_base_event_loop_t* loop, int timeout_ms) {
    if (!loop) return PPDB_BASE_ERR_PARAM;
    
    uint64_t start_time = ppdb_base_get_time_us();
    ppdb_error_t err = loop->ops->wait(loop->impl, timeout_ms);
    uint64_t end_time = ppdb_base_get_time_us();
    
    ppdb_base_mutex_lock(loop->mutex);
    loop->stats.total_wait_time_us += end_time - start_time;
    ppdb_base_mutex_unlock(loop->mutex);
    
    return err;
}

// Get event loop statistics
void ppdb_base_event_loop_get_stats(ppdb_base_event_loop_t* loop,
                                  uint64_t* total_events,
                                  uint64_t* total_errors,
                                  uint64_t* total_timeouts,
                                  uint64_t* total_wait_time_us,
                                  uint64_t* filtered_events) {
    if (!loop) return;
    
    ppdb_base_mutex_lock(loop->mutex);
    
    if (total_events) *total_events = loop->stats.total_events;
    if (total_errors) *total_errors = loop->stats.total_errors;
    if (total_timeouts) *total_timeouts = loop->stats.total_timeouts;
    if (total_wait_time_us) *total_wait_time_us = loop->stats.total_wait_time_us;
    if (filtered_events) *filtered_events = loop->stats.filtered_events;
    
    ppdb_base_mutex_unlock(loop->mutex);
}

//-----------------------------------------------------------------------------
// Timer System Implementation
//-----------------------------------------------------------------------------

// Global timer manager
static ppdb_timer_manager_t* timer_manager = NULL;

// Get high-precision time in nanoseconds
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Initialize timer manager
static ppdb_error_t timer_manager_init(void) {
    if (timer_manager) return PPDB_OK;

    timer_manager = ppdb_base_malloc(sizeof(ppdb_timer_manager_t));
    if (!timer_manager) return PPDB_BASE_ERR_MEMORY;

    memset(timer_manager, 0, sizeof(ppdb_timer_manager_t));

    ppdb_error_t err = ppdb_base_mutex_create(&timer_manager->mutex);
    if (err != PPDB_OK) {
        ppdb_base_free(timer_manager);
        timer_manager = NULL;
        return err;
    }

    timer_manager->current_time = get_time_ns();
    timer_manager->last_update = timer_manager->current_time;

    return PPDB_OK;
}

// Cleanup timer manager
static void timer_manager_cleanup(void) {
    if (!timer_manager) return;

    ppdb_base_mutex_lock(timer_manager->mutex);

    // Free all timers in wheels
    for (int i = 0; i < PPDB_TIMER_WHEEL_COUNT; i++) {
        for (int j = 0; j < PPDB_TIMER_WHEEL_SIZE; j++) {
            while (timer_manager->wheels[i].slots[j]) {
                ppdb_base_timer_t* timer = timer_manager->wheels[i].slots[j];
                timer_manager->wheels[i].slots[j] = timer->next;
                ppdb_base_free(timer);
            }
        }
    }

    // Free overflow timers
    while (timer_manager->overflow) {
        ppdb_base_timer_t* timer = timer_manager->overflow;
        timer_manager->overflow = timer->next;
        ppdb_base_free(timer);
    }

    // Free priority queue timers
    for (int i = 0; i < PPDB_TIMER_PRIORITY_COUNT; i++) {
        while (timer_manager->priority_queues[i]) {
            ppdb_base_timer_t* timer = timer_manager->priority_queues[i];
            timer_manager->priority_queues[i] = timer->next;
            ppdb_base_free(timer);
        }
    }

    ppdb_base_mutex_unlock(timer_manager->mutex);
    ppdb_base_mutex_destroy(timer_manager->mutex);
    ppdb_base_free(timer_manager);
    timer_manager = NULL;
}

// Add timer to wheel
static void add_timer_to_wheel(ppdb_base_timer_t* timer) {
    uint64_t expires = timer->next_timeout - timer_manager->current_time;
    uint32_t idx = 0;
    int wheel = 0;

    // Find appropriate wheel
    while (expires >= PPDB_TIMER_WHEEL_SIZE && wheel < PPDB_TIMER_WHEEL_COUNT - 1) {
        expires >>= PPDB_TIMER_WHEEL_BITS;
        wheel++;
    }

    // Add to wheel
    if (wheel < PPDB_TIMER_WHEEL_COUNT) {
        idx = (timer_manager->wheels[wheel].current + expires) & PPDB_TIMER_WHEEL_MASK;
        timer->next = timer_manager->wheels[wheel].slots[idx];
        timer_manager->wheels[wheel].slots[idx] = timer;
    } else {
        // Add to overflow list
        timer->next = timer_manager->overflow;
        timer_manager->overflow = timer;
    }
}

// Add timer to priority queue
static void add_timer_to_priority_queue(ppdb_base_timer_t* timer) {
    ppdb_base_timer_t** queue = &timer_manager->priority_queues[timer->priority];
    ppdb_base_timer_t** curr = queue;

    // Insert in order of next timeout
    while (*curr && (*curr)->next_timeout <= timer->next_timeout) {
        curr = &(*curr)->next;
    }

    timer->next = *curr;
    *curr = timer;
}

// Process expired timers
static void process_expired_timers(void) {
    uint64_t current_time = get_time_ns();
    timer_manager->current_time = current_time;

    // Process priority queues
    for (int i = 0; i < PPDB_TIMER_PRIORITY_COUNT; i++) {
        while (timer_manager->priority_queues[i] &&
               timer_manager->priority_queues[i]->next_timeout <= current_time) {
            ppdb_base_timer_t* timer = timer_manager->priority_queues[i];
            timer_manager->priority_queues[i] = timer->next;

            // Update statistics
            uint64_t elapsed = current_time - (timer->next_timeout - timer->interval_ns);
            timer->stats.last_elapsed = elapsed;
            timer->stats.total_elapsed += elapsed;
            timer->stats.total_ticks++;
            
            if (elapsed < timer->stats.min_elapsed || timer->stats.min_elapsed == 0) {
                timer->stats.min_elapsed = elapsed;
            }
            if (elapsed > timer->stats.max_elapsed) {
                timer->stats.max_elapsed = elapsed;
            }
            timer->stats.avg_elapsed = timer->stats.total_elapsed / timer->stats.total_ticks;

            // Calculate drift
            int64_t drift = (int64_t)(current_time - timer->next_timeout);
            timer->stats.drift += (drift < 0) ? -drift : drift;
            timer_manager->stats.total_drift += (drift < 0) ? -drift : drift;

            if (drift > timer->interval_ns) {
                timer_manager->stats.overdue_timers++;
            }

            // Call timer callback
            if (timer->callback) {
                timer->callback(timer, timer->user_data);
            }

            // Handle repeating timer
            if (timer->flags & PPDB_TIMER_FLAG_REPEAT) {
                // Adjust next timeout
                if (timer->flags & PPDB_TIMER_FLAG_PRECISE) {
                    timer->next_timeout += timer->interval_ns;
                } else {
                    timer->next_timeout = current_time + timer->interval_ns;
                }
                add_timer_to_priority_queue(timer);
            } else {
                timer_manager->stats.active_timers--;
                ppdb_base_free(timer);
            }
        }
    }

    // Update timer wheels
    uint64_t elapsed = current_time - timer_manager->last_update;
    timer_manager->last_update = current_time;

    for (int wheel = 0; wheel < PPDB_TIMER_WHEEL_COUNT; wheel++) {
        uint32_t increment = elapsed >> (wheel * PPDB_TIMER_WHEEL_BITS);
        if (increment == 0) break;

        ppdb_timer_wheel_t* w = &timer_manager->wheels[wheel];
        for (uint32_t i = 0; i < increment; i++) {
            uint32_t idx = (w->current + 1) & PPDB_TIMER_WHEEL_MASK;
            w->current = idx;

            // Move timers from current slot to priority queues
            while (w->slots[idx]) {
                ppdb_base_timer_t* timer = w->slots[idx];
                w->slots[idx] = timer->next;
                add_timer_to_priority_queue(timer);
            }
        }
    }

    // Check overflow list
    ppdb_base_timer_t** curr = &timer_manager->overflow;
    while (*curr) {
        ppdb_base_timer_t* timer = *curr;
        if (timer->next_timeout <= current_time + (PPDB_TIMER_WHEEL_SIZE << ((PPDB_TIMER_WHEEL_COUNT - 1) * PPDB_TIMER_WHEEL_BITS))) {
            *curr = timer->next;
            add_timer_to_wheel(timer);
        } else {
            curr = &timer->next;
        }
    }
}

// Create timer
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer,
                                  uint64_t interval_ns,
                                  uint32_t flags,
                                  int priority,
                                  ppdb_base_timer_callback_t callback,
                                  void* user_data) {
    if (!timer || !callback || interval_ns == 0 ||
        priority < 0 || priority >= PPDB_TIMER_PRIORITY_COUNT) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_error_t err = timer_manager_init();
    if (err != PPDB_OK) return err;

    ppdb_base_timer_t* new_timer = ppdb_base_malloc(sizeof(ppdb_base_timer_t));
    if (!new_timer) return PPDB_BASE_ERR_MEMORY;

    memset(new_timer, 0, sizeof(ppdb_base_timer_t));
    new_timer->interval_ns = interval_ns;
    new_timer->flags = flags;
    new_timer->priority = priority;
    new_timer->callback = callback;
    new_timer->user_data = user_data;

    ppdb_base_mutex_lock(timer_manager->mutex);

    new_timer->next_timeout = timer_manager->current_time + interval_ns;
    add_timer_to_wheel(new_timer);

    timer_manager->stats.total_timers++;
    timer_manager->stats.active_timers++;

    ppdb_base_mutex_unlock(timer_manager->mutex);

    *timer = new_timer;
    return PPDB_OK;
}

// Destroy timer
void ppdb_base_timer_destroy(ppdb_base_timer_t* timer) {
    if (!timer || !timer_manager) return;

    ppdb_base_mutex_lock(timer_manager->mutex);

    // Remove from wheels
    for (int i = 0; i < PPDB_TIMER_WHEEL_COUNT; i++) {
        for (int j = 0; j < PPDB_TIMER_WHEEL_SIZE; j++) {
            ppdb_base_timer_t** curr = &timer_manager->wheels[i].slots[j];
            while (*curr) {
                if (*curr == timer) {
                    *curr = timer->next;
                    timer_manager->stats.active_timers--;
                    ppdb_base_free(timer);
                    ppdb_base_mutex_unlock(timer_manager->mutex);
                    return;
                }
                curr = &(*curr)->next;
            }
        }
    }

    // Remove from overflow list
    ppdb_base_timer_t** curr = &timer_manager->overflow;
    while (*curr) {
        if (*curr == timer) {
            *curr = timer->next;
            timer_manager->stats.active_timers--;
            ppdb_base_free(timer);
            ppdb_base_mutex_unlock(timer_manager->mutex);
            return;
        }
        curr = &(*curr)->next;
    }

    // Remove from priority queues
    for (int i = 0; i < PPDB_TIMER_PRIORITY_COUNT; i++) {
        curr = &timer_manager->priority_queues[i];
        while (*curr) {
            if (*curr == timer) {
                *curr = timer->next;
                timer_manager->stats.active_timers--;
                ppdb_base_free(timer);
                ppdb_base_mutex_unlock(timer_manager->mutex);
                return;
            }
            curr = &(*curr)->next;
        }
    }

    ppdb_base_mutex_unlock(timer_manager->mutex);
}

// Get timer statistics
void ppdb_base_timer_get_stats(ppdb_base_timer_t* timer,
                             uint64_t* total_ticks,
                             uint64_t* min_elapsed,
                             uint64_t* max_elapsed,
                             uint64_t* avg_elapsed,
                             uint64_t* last_elapsed,
                             uint64_t* drift) {
    if (!timer) return;

    if (total_ticks) *total_ticks = timer->stats.total_ticks;
    if (min_elapsed) *min_elapsed = timer->stats.min_elapsed;
    if (max_elapsed) *max_elapsed = timer->stats.max_elapsed;
    if (avg_elapsed) *avg_elapsed = timer->stats.avg_elapsed;
    if (last_elapsed) *last_elapsed = timer->stats.last_elapsed;
    if (drift) *drift = timer->stats.drift;
}

// Get timer manager statistics
void ppdb_base_timer_get_manager_stats(uint64_t* total_timers,
                                    uint64_t* active_timers,
                                    uint64_t* expired_timers,
                                    uint64_t* overdue_timers,
                                    uint64_t* total_drift) {
    if (!timer_manager) return;

    ppdb_base_mutex_lock(timer_manager->mutex);

    if (total_timers) *total_timers = timer_manager->stats.total_timers;
    if (active_timers) *active_timers = timer_manager->stats.active_timers;
    if (expired_timers) *expired_timers = timer_manager->stats.expired_timers;
    if (overdue_timers) *overdue_timers = timer_manager->stats.overdue_timers;
    if (total_drift) *total_drift = timer_manager->stats.total_drift;

    ppdb_base_mutex_unlock(timer_manager->mutex);
}

// Update timers
void ppdb_base_timer_update(void) {
    if (!timer_manager) return;

    ppdb_base_mutex_lock(timer_manager->mutex);
    process_expired_timers();
    ppdb_base_mutex_unlock(timer_manager->mutex);
} 