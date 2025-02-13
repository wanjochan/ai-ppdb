#include "internal/polyx/PolyxAsync.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
// #include <sys/timerfd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
// #include <poll.h>

// 全局内存管理器
static InfraxMemory* g_memory = NULL;
extern InfraxMemoryClassType InfraxMemoryClass;

// 全局 Core 实例
static InfraxCore* g_core = NULL;

// 建议的重构方向：
// PolyxAsync 应该只保留最基础的异步原语
// 把 file_* 相关移到 PolyxIO
// 把 http_* 相关移到 PolyxNet
// 每个模块都可以基于 PolyxAsync 的基础设施来实现自己的异步操作

// TODO: InfraxAsync 性能优化完成后再实现这些函数
// 目前主要问题：
// 1. 底层 InfraxAsync 的性能问题需要先解决
// 2. 异步任务的调度和执行效率需要提升
// 3. 完成底层优化后再考虑具体实现或重构方案

// Internal callback functions for InfraxAsync
// These functions are intended to be the actual implementations of async operations
// but implementation is pending InfraxAsync performance optimization

// File operation callbacks
void async_read_file_fn(InfraxAsync* async, void* arg);
void async_write_file_fn(InfraxAsync* async, void* arg);

// HTTP operation callbacks
void async_http_get_fn(InfraxAsync* async, void* arg);
void async_http_post_fn(InfraxAsync* async, void* arg);

// Timer operation callbacks
void async_delay_fn(InfraxAsync* async, void* arg);
void async_interval_fn(InfraxAsync* async, void* arg);

// Task composition callbacks
void async_parallel_fn(InfraxAsync* async, void* arg);
void async_sequence_fn(InfraxAsync* async, void* arg);

// 实例方法声明
PolyxAsync* polyx_async_start(PolyxAsync* self);
void polyx_async_cancel(PolyxAsync* self);
bool polyx_async_is_done(PolyxAsync* self);
void* polyx_async_get_result(PolyxAsync* self, size_t* size);

// 并行和序列执行方法声明
PolyxAsync* polyx_async_parallel_start(PolyxAsync* self);
void polyx_async_parallel_cancel(PolyxAsync* self);
bool polyx_async_parallel_is_done(PolyxAsync* self);
void* polyx_async_parallel_get_result(PolyxAsync* self, size_t* size);

PolyxAsync* polyx_async_sequence_start(PolyxAsync* self);
void polyx_async_sequence_cancel(PolyxAsync* self);
bool polyx_async_sequence_is_done(PolyxAsync* self);
void* polyx_async_sequence_get_result(PolyxAsync* self, size_t* size);

// 任务结构定义
typedef struct {
    char* path;
} FileReadTask;

typedef struct {
    char* path;
    void* data;
    size_t size;
} FileWriteTask;

typedef struct {
    char* url;
} HttpGetTask;

typedef struct {
    char* url;
    void* data;
    size_t size;
} HttpPostTask;

typedef struct {
    int ms;
} DelayTask;

typedef struct {
    int ms;
    int count;
    int current;
} IntervalTask;

// 并行和序列执行数据结构
typedef struct {
    PolyxAsync** tasks;
    int count;
    int completed;
    int current;
} ParallelSequenceData;

// Timer callback type
typedef void (*TimerCallback)(void* arg);
typedef void (*EventCallback)(PolyxEvent* event, void* arg);

// Timer data structure
typedef struct {
    int64_t interval_ms;    // Timer interval
    int64_t next_trigger;   // Next trigger time
    bool is_periodic;       // Whether timer repeats
    TimerCallback callback; // Timer callback
    void* arg;             // Callback argument
} TimerData;

// Event structure extension
typedef struct {
    PolyxEvent base;
    int read_fd;
    int write_fd;
    void* callback;  // TimerCallback or EventCallback
    void* arg;
    void* data;
    size_t data_size;
} PolyxEventInternal;

// Internal context structure
typedef struct {
    InfraxMemory* memory;
    InfraxAsync* infra;
    void* private_data;
    void (*cleanup_fn)(void*);
    struct pollfd* fds;
    size_t fds_count;
    size_t fds_capacity;
    TimerData** timers;
    size_t timers_count;
    size_t timers_capacity;
} PolyxAsyncContext;

// 清理函数定义
static void file_read_task_cleanup(void* private_data) {
    FileReadTask* task = (FileReadTask*)private_data;
    if (!task) return;
    if (task->path) {
        g_memory->dealloc(g_memory, task->path);
    }
    g_memory->dealloc(g_memory, task);
}

static void file_write_task_cleanup(void* private_data) {
    FileWriteTask* task = (FileWriteTask*)private_data;
    if (!task) return;
    if (task->path) {
        g_memory->dealloc(g_memory, task->path);
    }
    if (task->data) {
        g_memory->dealloc(g_memory, task->data);
    }
    g_memory->dealloc(g_memory, task);
}

static void http_get_task_cleanup(void* private_data) {
    HttpGetTask* task = (HttpGetTask*)private_data;
    if (!task) return;
    if (task->url) {
        g_memory->dealloc(g_memory, task->url);
    }
    g_memory->dealloc(g_memory, task);
}

static void http_post_task_cleanup(void* private_data) {
    HttpPostTask* task = (HttpPostTask*)private_data;
    if (!task) return;
    if (task->url) {
        g_memory->dealloc(g_memory, task->url);
    }
    if (task->data) {
        g_memory->dealloc(g_memory, task->data);
    }
    g_memory->dealloc(g_memory, task);
}

static void delay_task_cleanup(void* private_data) {
    DelayTask* task = (DelayTask*)private_data;
    if (!task) return;
    g_memory->dealloc(g_memory, task);
}

static void interval_task_cleanup(void* private_data) {
    IntervalTask* task = (IntervalTask*)private_data;
    if (!task) return;
    g_memory->dealloc(g_memory, task);
}

static void parallel_sequence_task_cleanup(void* private_data) {
    ParallelSequenceData* data = (ParallelSequenceData*)private_data;
    if (!data) return;
    if (data->tasks) {
        g_memory->dealloc(g_memory, data->tasks);
    }
    g_memory->dealloc(g_memory, data);
}

// 异步操作结构体
bool init_memory() {
    if (g_memory) return true;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    g_core = InfraxCoreClass.singleton();
    
    if (!g_memory || !g_core) {
        if (g_memory) {
            InfraxMemoryClass.free(g_memory);
            g_memory = NULL;
        }
        return false;
    }
    
    return true;
}

// Forward declarations of internal functions
static PolyxAsync* polyx_async_new(void);
static void polyx_async_free(PolyxAsync* self);
static PolyxEvent* polyx_async_create_event(PolyxAsync* self, PolyxEventConfig* config);
static void polyx_async_trigger_event(PolyxAsync* self, PolyxEvent* event, void* data, size_t size);
static void polyx_async_destroy_event(PolyxAsync* self, PolyxEvent* event);
static PolyxEvent* polyx_async_create_timer(PolyxAsync* self, PolyxTimerConfig* config);
static void polyx_async_start_timer(PolyxAsync* self, PolyxEvent* timer);
static void polyx_async_stop_timer(PolyxAsync* self, PolyxEvent* timer);
static int polyx_async_poll(PolyxAsync* self, int timeout_ms);

// Network related functions
static PolyxEvent* polyx_async_create_tcp_event(PolyxAsync* self, PolyxNetworkConfig* config);
static PolyxEvent* polyx_async_create_udp_event(PolyxAsync* self, PolyxNetworkConfig* config);
static PolyxEvent* polyx_async_create_unix_event(PolyxAsync* self, PolyxNetworkConfig* config);

// IO related functions
static PolyxEvent* polyx_async_create_pipe_event(PolyxAsync* self, PolyxIOConfig* config);
static PolyxEvent* polyx_async_create_fifo_event(PolyxAsync* self, PolyxIOConfig* config);
static PolyxEvent* polyx_async_create_tty_event(PolyxAsync* self, PolyxIOConfig* config);

// File monitoring functions
static PolyxEvent* polyx_async_create_inotify_event(PolyxAsync* self, PolyxInotifyConfig* config);

// Statistics functions
static void polyx_async_get_stats(PolyxAsync* self, PolyxEventStats* stats);
static void polyx_async_reset_stats(PolyxAsync* self);

// Debug functions
static void polyx_async_set_debug_level(PolyxAsync* self, PolyxDebugLevel level);
static void polyx_async_set_debug_callback(PolyxAsync* self, PolyxDebugCallback callback, void* context);

// Event group functions
static int polyx_async_create_event_group(PolyxAsync* self, PolyxEvent** events, size_t count);
static int polyx_async_wait_event_group(PolyxAsync* self, int group_id, int timeout_ms);
static void polyx_async_destroy_event_group(PolyxAsync* self, int group_id);

// Timer callback wrapper
static void timer_callback_wrapper(InfraxAsync* async, int fd, short events, void* arg) {
    PolyxEventInternal* timer = (PolyxEventInternal*)arg;
    if (!timer || !timer->callback) return;
    
    TimerData* timer_data = (TimerData*)timer->data;
    if (!timer_data) return;
    
    // Clear the pipe
    char dummy;
    read(fd, &dummy, 1);
    
    // Call the callback
    TimerCallback callback = (TimerCallback)timer->callback;
    callback(timer->arg);
    
    // If periodic, schedule next trigger
    if (timer_data->is_periodic) {
        timer_data->next_trigger += timer_data->interval_ms;
        // Write to pipe to trigger next callback
        write(timer->write_fd, &dummy, 1);
    }
}

// Event callback wrapper
static void event_callback_wrapper(InfraxAsync* async, int fd, short events, void* arg) {
    PolyxEventInternal* event = (PolyxEventInternal*)arg;
    if (!event || !event->callback) return;
    
    // 每次只读取一个事件的数据
    char buffer[1024];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read <= 0) return;
    
    // 调用用户回调
    EventCallback callback = (EventCallback)event->callback;
    callback((PolyxEvent*)event, event->arg);
}

// Dummy function for InfraxAsync instance
static void dummy_fn(InfraxAsync* async, void* arg) {
    // Do nothing
}

// 全局类实例
const PolyxAsyncClassType PolyxAsyncClass = {
    .new = polyx_async_new,
    .free = polyx_async_free,
    .create_event = polyx_async_create_event,
    .create_timer = polyx_async_create_timer,
    .destroy_event = polyx_async_destroy_event,
    .trigger_event = polyx_async_trigger_event,
    .start_timer = polyx_async_start_timer,
    .stop_timer = polyx_async_stop_timer,
    .poll = polyx_async_poll,
    
    // Network related methods
    .create_tcp_event = polyx_async_create_tcp_event,
    .create_udp_event = polyx_async_create_udp_event,
    .create_unix_event = polyx_async_create_unix_event,
    
    // IO related methods
    .create_pipe_event = polyx_async_create_pipe_event,
    .create_fifo_event = polyx_async_create_fifo_event,
    .create_tty_event = polyx_async_create_tty_event,
    
    // File monitoring methods
    .create_inotify_event = polyx_async_create_inotify_event,
    
    // Statistics methods
    .get_stats = polyx_async_get_stats,
    .reset_stats = polyx_async_reset_stats,
    
    // Debug methods
    .set_debug_level = polyx_async_set_debug_level,
    .set_debug_callback = polyx_async_set_debug_callback,
    
    // Event group methods
    .create_event_group = polyx_async_create_event_group,
    .wait_event_group = polyx_async_wait_event_group,
    .destroy_event_group = polyx_async_destroy_event_group
};

// Private event group structure
typedef struct {
    int id;
    PolyxEvent** events;
    size_t count;
} EventGroup;

// Private data for PolyxAsync
typedef struct {
    EventGroup* groups;
    size_t group_count;
    size_t group_capacity;
    InfraxMemory* memory;  // Memory manager instance
} PolyxAsyncPrivate;

// Helper functions
static PolyxAsyncPrivate* create_private_data(void) {
    printf("Enter create_private_data\n");
    
    // Create memory manager
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    printf("Creating memory manager\n");
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    if (!memory) {
        printf("Failed to create memory manager\n");
        return NULL;
    }
    
    // Allocate private data structure
    printf("Allocating private data structure\n");
    PolyxAsyncPrivate* private = memory->alloc(memory, sizeof(PolyxAsyncPrivate));
    if (!private) {
        printf("Failed to allocate private data structure\n");
        InfraxMemoryClass.free(memory);
        return NULL;
    }
    
    // Initialize private data
    printf("Initializing private data\n");
    memset(private, 0, sizeof(PolyxAsyncPrivate));
    private->memory = memory;  // Store memory manager for later use
    
    // Initialize event groups array
    printf("Initializing event groups array\n");
    private->group_capacity = 8;  // Initial capacity
    private->groups = memory->alloc(memory, private->group_capacity * sizeof(EventGroup));
    if (!private->groups) {
        printf("Failed to allocate event groups array\n");
        memory->dealloc(memory, private);
        InfraxMemoryClass.free(memory);
        return NULL;
    }
    
    // Initialize each event group
    printf("Initializing event groups\n");
    for (size_t i = 0; i < private->group_capacity; i++) {
        private->groups[i].id = -1;  // Invalid ID
        private->groups[i].events = NULL;
        private->groups[i].count = 0;
    }
    
    printf("Private data created successfully\n");
    return private;
}

static void destroy_private_data(PolyxAsyncPrivate* private) {
    if (!private) return;
    
    InfraxMemory* memory = private->memory;
    if (!memory) return;
    
    if (private->groups) {
        for (size_t i = 0; i < private->group_count; i++) {
            if (private->groups[i].events) {
                memory->dealloc(memory, private->groups[i].events);
            }
        }
        memory->dealloc(memory, private->groups);
    }
    
    memory->dealloc(memory, private);
    InfraxMemoryClass.free(memory);
}

// Callback functions
static void polyx_async_poll_callback(InfraxAsync* async, int fd, short events, void* arg) {
    PolyxAsync* self = (PolyxAsync*)arg;
    if (!self) return;
    
    // Find the event with this fd
    for (size_t i = 0; i < self->event_count; i++) {
        PolyxEvent* event = self->events[i];
        if (!event) continue;
        
        int event_fd = polyx_event_get_fd(event);
        if (event_fd == fd) {
            // Update event status
            if (events & POLLIN) {
                event->status = POLYX_EVENT_STATUS_ACTIVE;
                if (event->callback) {
                    event->callback(event, event->arg);
                }
            }
            break;
        }
    }
}

static void polyx_async_async_callback(InfraxAsync* async, void* arg) {
    PolyxAsync* self = (PolyxAsync*)arg;
    if (!self) return;
    
    // Handle async events
    // This is called when the async operation completes
}

// Implementation functions
static PolyxAsync* polyx_async_new(void) {
    printf("Enter polyx_async_new\n");
    
    // Create memory manager for PolyxAsync instance
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    printf("Creating memory manager\n");
    InfraxMemory* memory = InfraxMemoryClass.new(&config);
    if (!memory) {
        printf("Failed to create memory manager\n");
        return NULL;
    }
    
    printf("Allocating PolyxAsync structure\n");
    PolyxAsync* self = memory->alloc(memory, sizeof(PolyxAsync));
    if (!self) {
        printf("Failed to allocate PolyxAsync structure\n");
        InfraxMemoryClass.free(memory);
        return NULL;
    }
    
    printf("Initializing PolyxAsync structure\n");
    memset(self, 0, sizeof(PolyxAsync));  // Clear all fields
    self->self = self;
    self->klass = &PolyxAsyncClass;
    
    printf("Creating InfraxAsync instance\n");
    self->infrax = InfraxAsyncClass.new(polyx_async_async_callback, self);
    if (!self->infrax) {
        printf("Failed to create InfraxAsync instance\n");
        memory->dealloc(memory, self);
        InfraxMemoryClass.free(memory);
        return NULL;
    }
    
    printf("Starting InfraxAsync instance\n");
    if (!InfraxAsyncClass.start(self->infrax)) {
        printf("Failed to start InfraxAsync instance\n");
        InfraxAsyncClass.free(self->infrax);
        memory->dealloc(memory, self);
        InfraxMemoryClass.free(memory);
        return NULL;
    }
    
    printf("Creating private data\n");
    PolyxAsyncPrivate* private = memory->alloc(memory, sizeof(PolyxAsyncPrivate));
    if (!private) {
        printf("Failed to allocate private data\n");
        InfraxAsyncClass.free(self->infrax);
        memory->dealloc(memory, self);
        InfraxMemoryClass.free(memory);
        return NULL;
    }
    
    // Initialize private data
    printf("Initializing private data\n");
    memset(private, 0, sizeof(PolyxAsyncPrivate));
    private->memory = memory;  // Store memory manager for later use
    
    // Initialize event groups array
    printf("Initializing event groups array\n");
    private->group_capacity = 8;  // Initial capacity
    private->groups = memory->alloc(memory, private->group_capacity * sizeof(EventGroup));
    if (!private->groups) {
        printf("Failed to allocate event groups array\n");
        memory->dealloc(memory, private);
        InfraxAsyncClass.free(self->infrax);
        memory->dealloc(memory, self);
        InfraxMemoryClass.free(memory);
        return NULL;
    }
    
    // Initialize each event group
    printf("Initializing event groups\n");
    for (size_t i = 0; i < private->group_capacity; i++) {
        private->groups[i].id = -1;  // Invalid ID
        private->groups[i].events = NULL;
        private->groups[i].count = 0;
    }
    
    self->private_data = private;
    printf("PolyxAsync instance created successfully\n");
    return self;
}

static void polyx_async_free(PolyxAsync* self) {
    if (!self) return;
    
    printf("Enter polyx_async_free\n");
    
    PolyxAsyncPrivate* private = self->private_data;
    if (!private) {
        printf("Private data is NULL\n");
        return;
    }
    
    InfraxMemory* memory = private->memory;
    if (!memory) {
        printf("Memory manager is NULL\n");
        return;
    }
    
    printf("Freeing events\n");
    // Free all events
    if (self->events) {
        for (size_t i = 0; i < self->event_count; i++) {
            if (self->events[i]) {
                printf("Freeing event %zu\n", i);
                memory->dealloc(memory, self->events[i]);
            }
        }
        printf("Freeing events array\n");
        memory->dealloc(memory, self->events);
        self->events = NULL;
        self->event_count = 0;
        self->event_capacity = 0;
    }
    
    printf("Freeing event groups\n");
    // Free event groups
    if (private->groups) {
        for (size_t i = 0; i < private->group_count; i++) {
            if (private->groups[i].events) {
                printf("Freeing group %zu events\n", i);
                memory->dealloc(memory, private->groups[i].events);
            }
        }
        printf("Freeing groups array\n");
        memory->dealloc(memory, private->groups);
        private->groups = NULL;
        private->group_count = 0;
        private->group_capacity = 0;
    }
    
    printf("Freeing InfraxAsync instance\n");
    // Free infrax
    if (self->infrax) {
        InfraxAsyncClass.free(self->infrax);
        self->infrax = NULL;
    }
    
    printf("Freeing private data\n");
    // Free private data
    memory->dealloc(memory, private);
    self->private_data = NULL;
    
    printf("Freeing self\n");
    // Free self
    memory->dealloc(memory, self);
    
    printf("Freeing memory manager\n");
    // Finally, free the memory manager
    InfraxMemoryClass.free(memory);
    
    printf("PolyxAsync instance freed successfully\n");
}

static PolyxEvent* polyx_async_create_event(PolyxAsync* self, PolyxEventConfig* config) {
    printf("Enter create_event\n");
    if (!self || !config) {
        printf("Invalid parameters: self=%p, config=%p\n", (void*)self, (void*)config);
        return NULL;
    }
    
    printf("Getting private data\n");
    PolyxAsyncPrivate* private = (PolyxAsyncPrivate*)self->private_data;
    if (!private) {
        printf("Private data is NULL\n");
        return NULL;
    }
    
    printf("Getting memory manager\n");
    InfraxMemory* memory = private->memory;
    if (!memory) {
        printf("Memory manager is NULL\n");
        return NULL;
    }
    
    // Initialize event
    printf("Allocating event structure\n");
    PolyxEvent* event = memory->alloc(memory, sizeof(PolyxEvent));
    if (!event) {
        printf("Failed to allocate event structure\n");
        return NULL;
    }
    
    printf("Initializing event structure\n");
    memset(event, 0, sizeof(PolyxEvent));  // Clear all fields
    
    // Set event properties
    event->type = config->type;
    event->status = POLYX_EVENT_STATUS_INIT;
    event->last_error = POLYX_OK;
    event->callback = config->callback;
    event->arg = config->arg;
    
    // Initialize event data based on type
    switch (event->type) {
        case POLYX_EVENT_IO:
            event->data.io.fd = -1;
            event->data.io.events = 0;
            break;
        case POLYX_EVENT_TIMER:
            event->data.timer.due_time = 0;
            event->data.timer.callback = NULL;
            break;
        case POLYX_EVENT_TCP:
        case POLYX_EVENT_UDP:
        case POLYX_EVENT_UNIX:
            event->data.network.socket_fd = -1;
            event->data.network.events = 0;
            event->data.network.protocol = 0;
            break;
        case POLYX_EVENT_INOTIFY:
            event->data.inotify.watch_fd = -1;
            event->data.inotify.path = NULL;
            break;
        default:
            break;
    }
    
    // Add to events array
    printf("Adding event to events array\n");
    if (!self->events) {
        printf("Creating initial events array\n");
        self->event_capacity = 8;  // Initial capacity
        self->events = memory->alloc(memory, self->event_capacity * sizeof(PolyxEvent*));
        if (!self->events) {
            printf("Failed to allocate initial events array\n");
            memory->dealloc(memory, event);
            return NULL;
        }
        self->event_count = 0;
    } else if (self->event_count >= self->event_capacity) {
        printf("Expanding events array (count=%zu, capacity=%zu)\n", 
               self->event_count, self->event_capacity);
        size_t new_capacity = self->event_capacity * 2;
        PolyxEvent** new_events = memory->alloc(memory, new_capacity * sizeof(PolyxEvent*));
        if (!new_events) {
            printf("Failed to allocate new events array\n");
            memory->dealloc(memory, event);
            return NULL;
        }
        
        printf("Copying existing events\n");
        memcpy(new_events, self->events, self->event_count * sizeof(PolyxEvent*));
        memory->dealloc(memory, self->events);
        self->events = new_events;
        self->event_capacity = new_capacity;
        printf("Events array expanded (new_capacity=%zu)\n", new_capacity);
    }
    
    printf("Storing event in array\n");
    self->events[self->event_count++] = event;
    
    // Initialize statistics if needed
    if (self->stats.total_events == 0) {
        memset(&self->stats, 0, sizeof(PolyxEventStats));
    }
    
    self->stats.total_events++;
    self->stats.active_events++;
    
    printf("Updating event type statistics\n");
    switch (event->type) {
        case POLYX_EVENT_IO:
            self->stats.by_type.io_events++;
            break;
        case POLYX_EVENT_TIMER:
            self->stats.by_type.timer_events++;
            break;
        case POLYX_EVENT_TCP:
        case POLYX_EVENT_UDP:
        case POLYX_EVENT_UNIX:
            self->stats.by_type.network_events++;
            break;
        case POLYX_EVENT_INOTIFY:
            self->stats.by_type.monitor_events++;
            break;
        default:
            break;
    }
    
    printf("Event created successfully\n");
    return event;
}

static void polyx_async_destroy_event(PolyxAsync* self, PolyxEvent* event) {
    if (!self || !event) return;
    
    PolyxAsyncPrivate* private = self->private_data;
    if (!private || !private->memory) return;
    
    InfraxMemory* memory = private->memory;
    
    // Remove from events array
    for (size_t i = 0; i < self->event_count; i++) {
        if (self->events[i] == event) {
            memmove(&self->events[i], &self->events[i + 1], 
                   (self->event_count - i - 1) * sizeof(PolyxEvent*));
            self->event_count--;
            break;
        }
    }
    
    memory->dealloc(memory, event);
}

static void polyx_async_trigger_event(PolyxAsync* self, PolyxEvent* event, void* data, size_t size) {
    if (!self || !event) return;
    
    if (event->callback) {
        event->callback(event, event->arg);
    }
}

static PolyxEvent* polyx_async_create_timer(PolyxAsync* self, PolyxTimerConfig* config) {
    if (!self || !config) return NULL;
    
    PolyxEventConfig event_config = {
        .type = POLYX_EVENT_TIMER,
        .callback = NULL,
        .arg = config->arg
    };
    
    PolyxEvent* event = polyx_async_create_event(self, &event_config);
    if (!event) return NULL;
    
    event->data.timer.due_time = 0;  // Will be set when timer starts
    event->data.timer.callback = config->callback;
    
    return event;
}

static void polyx_async_start_timer(PolyxAsync* self, PolyxEvent* timer) {
    if (!self || !timer || timer->type != POLYX_EVENT_TIMER) return;
    
    // TODO: Implement timer start
}

static void polyx_async_stop_timer(PolyxAsync* self, PolyxEvent* timer) {
    if (!self || !timer || timer->type != POLYX_EVENT_TIMER) return;
    
    // TODO: Implement timer stop
}

static int polyx_async_poll(PolyxAsync* self, int timeout_ms) {
    if (!self) return POLYX_ERROR_INVALID_PARAM;
    
    // TODO: Implement polling
    return POLYX_OK;
}

static void polyx_async_get_stats(PolyxAsync* self, PolyxEventStats* stats) {
    if (!self || !stats) return;
    memcpy(stats, &self->stats, sizeof(PolyxEventStats));
}

static void polyx_async_reset_stats(PolyxAsync* self) {
    if (!self) return;
    memset(&self->stats, 0, sizeof(PolyxEventStats));
}

static void polyx_async_set_debug_level(PolyxAsync* self, PolyxDebugLevel level) {
    if (!self) return;
    self->debug_level = level;
}

static void polyx_async_set_debug_callback(PolyxAsync* self, PolyxDebugCallback callback, void* context) {
    if (!self) return;
    self->debug_callback = callback;
    self->debug_context = context;
}

static int polyx_async_create_event_group(PolyxAsync* self, PolyxEvent** events, size_t count) {
    if (!self || !events || !count) return -1;
    
    PolyxAsyncPrivate* private = self->private_data;
    if (!private || !private->memory) return -1;
    
    InfraxMemory* memory = private->memory;
    
    // Expand groups array if needed
    if (private->group_count >= private->group_capacity) {
        size_t new_capacity = private->group_capacity ? private->group_capacity * 2 : 8;
        EventGroup* new_groups = memory->alloc(memory, new_capacity * sizeof(EventGroup));
        if (!new_groups) return -1;
        
        if (private->groups) {
            memcpy(new_groups, private->groups, private->group_count * sizeof(EventGroup));
            memory->dealloc(memory, private->groups);
        }
        private->groups = new_groups;
        private->group_capacity = new_capacity;
    }
    
    // Create new group
    EventGroup* group = &private->groups[private->group_count];
    group->id = private->group_count + 1;
    group->events = memory->alloc(memory, count * sizeof(PolyxEvent*));
    if (!group->events) return -1;
    
    memcpy(group->events, events, count * sizeof(PolyxEvent*));
    group->count = count;
    
    private->group_count++;
    return group->id;
}

static int polyx_async_wait_event_group(PolyxAsync* self, int group_id, int timeout_ms) {
    if (!self || group_id <= 0) return POLYX_ERROR_INVALID_PARAM;
    
    PolyxAsyncPrivate* private = self->private_data;
    if (!private) return POLYX_ERROR_INVALID_PARAM;
    
    // Find group
    EventGroup* group = NULL;
    for (size_t i = 0; i < private->group_count; i++) {
        if (private->groups[i].id == group_id) {
            group = &private->groups[i];
            break;
        }
    }
    if (!group) return POLYX_ERROR_INVALID_PARAM;
    
    // Check if any event is ready
    for (size_t i = 0; i < group->count; i++) {
        if (group->events[i]->status == POLYX_EVENT_STATUS_ACTIVE) {
            return POLYX_OK;
        }
    }
    
    return POLYX_ERROR_TIMEOUT;
}

static void polyx_async_destroy_event_group(PolyxAsync* self, int group_id) {
    if (!self || group_id <= 0) return;
    
    PolyxAsyncPrivate* private = self->private_data;
    if (!private || !private->memory) return;
    
    InfraxMemory* memory = private->memory;
    
    // Find and remove group
    for (size_t i = 0; i < private->group_count; i++) {
        if (private->groups[i].id == group_id) {
            memory->dealloc(memory, private->groups[i].events);
            memmove(&private->groups[i], &private->groups[i + 1],
                   (private->group_count - i - 1) * sizeof(EventGroup));
            private->group_count--;
            break;
        }
    }
}

// Network event creation
static PolyxEvent* polyx_async_create_tcp_event(PolyxAsync* self, PolyxNetworkConfig* config) {
    if (!self || !config) return NULL;
    
    PolyxEventConfig base_config = {
        .type = POLYX_EVENT_TCP,
        .callback = config->callback,
        .arg = config->arg
    };
    
    PolyxEvent* event = polyx_async_create_event(self, &base_config);
    if (!event) return NULL;
    
    event->data.network.socket_fd = config->socket_fd;
    event->data.network.events = config->events;
    event->data.network.protocol = IPPROTO_TCP;
    
    self->stats.by_type.network_events++;
    return event;
}

// Stub implementations for unimplemented functions
static PolyxEvent* polyx_async_create_udp_event(PolyxAsync* self, PolyxNetworkConfig* config) {
    return NULL;  // Not implemented yet
}

static PolyxEvent* polyx_async_create_unix_event(PolyxAsync* self, PolyxNetworkConfig* config) {
    return NULL;  // Not implemented yet
}

static PolyxEvent* polyx_async_create_pipe_event(PolyxAsync* self, PolyxIOConfig* config) {
    return NULL;  // Not implemented yet
}

static PolyxEvent* polyx_async_create_fifo_event(PolyxAsync* self, PolyxIOConfig* config) {
    return NULL;  // Not implemented yet
}

static PolyxEvent* polyx_async_create_tty_event(PolyxAsync* self, PolyxIOConfig* config) {
    return NULL;  // Not implemented yet
}

static PolyxEvent* polyx_async_create_inotify_event(PolyxAsync* self, PolyxInotifyConfig* config) {
    return NULL;  // Not implemented yet
}
