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
static void polyx_async_free(PolyxAsync* self);
static PolyxEvent* polyx_async_create_event(PolyxAsync* self, PolyxEventConfig* config);
static void polyx_async_trigger_event(PolyxAsync* self, PolyxEvent* event, void* data, size_t size);
static void polyx_async_destroy_event(PolyxAsync* self, PolyxEvent* event);
static PolyxEvent* polyx_async_create_timer(PolyxAsync* self, PolyxTimerConfig* config);
static void polyx_async_start_timer(PolyxAsync* self, PolyxEvent* timer);
static void polyx_async_stop_timer(PolyxAsync* self, PolyxEvent* timer);
static int polyx_async_poll(PolyxAsync* self, int timeout_ms);
static void dummy_fn(InfraxAsync* async, void* arg);

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

// Create new PolyxAsync instance
static PolyxAsync* polyx_async_new(void) {
    // Initialize memory first
    if (!init_memory()) {
        return NULL;
    }
    
    PolyxAsync* self = (PolyxAsync*)g_memory->alloc(g_memory, sizeof(PolyxAsync));
    if (!self) return NULL;
    
    memset(self, 0, sizeof(PolyxAsync));
    
    // Create InfraxAsync instance with a dummy function
    self->infrax = InfraxAsyncClass.new(dummy_fn, NULL);
    if (!self->infrax) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    
    // Initialize events array
    self->event_capacity = 32;
    self->events = (PolyxEvent**)g_memory->alloc(g_memory, sizeof(PolyxEvent*) * self->event_capacity);
    if (!self->events) {
        InfraxAsyncClass.free(self->infrax);
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    self->event_count = 0;
    
    // Initialize instance methods
    self->create_event = polyx_async_create_event;
    self->create_timer = polyx_async_create_timer;
    self->destroy_event = polyx_async_destroy_event;
    self->start_timer = polyx_async_start_timer;
    self->stop_timer = polyx_async_stop_timer;
    self->trigger_event = polyx_async_trigger_event;
    self->poll = polyx_async_poll;
    
    return self;
}

// Free PolyxAsync instance
static void polyx_async_free(PolyxAsync* self) {
    if (!self) return;
    
    // Clean up events
    for (size_t i = 0; i < self->event_count; i++) {
        polyx_async_destroy_event(self, self->events[i]);
    }
    
    // Free events array
    if (self->events) {
        g_memory->dealloc(g_memory, self->events);
    }
    
    // Free InfraxAsync instance
    if (self->infrax) {
        InfraxAsyncClass.free(self->infrax);
    }
    
    g_memory->dealloc(g_memory, self);
}

// Create event
static PolyxEvent* polyx_async_create_event(PolyxAsync* self, PolyxEventConfig* config) {
    if (!self || !config) return NULL;
    
    PolyxEventInternal* event = (PolyxEventInternal*)g_memory->alloc(g_memory, sizeof(PolyxEventInternal));
    if (!event) return NULL;
    
    memset(event, 0, sizeof(PolyxEventInternal));
    
    // Create pipe for event communication
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        g_memory->dealloc(g_memory, event);
        return NULL;
    }
    
    // Set non-blocking mode
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    
    event->base.type = config->type;
    event->read_fd = pipefd[0];
    event->write_fd = pipefd[1];
    event->callback = config->callback;
    event->arg = config->arg;
    
    // Add read end to pollset
    if (InfraxAsyncClass.pollset_add_fd(self->infrax, event->read_fd, INFRAX_POLLIN, event_callback_wrapper, event) < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        g_memory->dealloc(g_memory, event);
        return NULL;
    }
    
    // Add to events array
    if (self->event_count >= self->event_capacity) {
        size_t new_capacity = self->event_capacity * 2;
        PolyxEvent** new_events = (PolyxEvent**)g_memory->alloc(g_memory, sizeof(PolyxEvent*) * new_capacity);
        if (!new_events) {
            close(pipefd[0]);
            close(pipefd[1]);
            g_memory->dealloc(g_memory, event);
            return NULL;
        }
        memcpy(new_events, self->events, sizeof(PolyxEvent*) * self->event_count);
        g_memory->dealloc(g_memory, self->events);
        self->events = new_events;
        self->event_capacity = new_capacity;
    }
    
    self->events[self->event_count++] = (PolyxEvent*)event;
    
    return (PolyxEvent*)event;
}

// Trigger event
static void polyx_async_trigger_event(PolyxAsync* self, PolyxEvent* event, void* data, size_t size) {
    if (!self || !event) return;
    
    PolyxEventInternal* internal = (PolyxEventInternal*)event;
    
    // 清空pipe中可能存在的旧数据
    char buffer[1024];
    while (read(internal->read_fd, buffer, sizeof(buffer)) > 0) {
        // 丢弃旧数据
    }
    
    // Write event data to pipe
    ssize_t written = write(internal->write_fd, data, size);
    if (written < 0) {
        // 写入失败，记录错误
        InfraxLog* log = InfraxLogClass.singleton();
        log->error(log, "Failed to write event data: %s", strerror(errno));
    }
}

// Destroy event
static void polyx_async_destroy_event(PolyxAsync* self, PolyxEvent* event) {
    if (!self || !event) return;
    
    PolyxEventInternal* internal = (PolyxEventInternal*)event;
    
    // Remove from pollset
    InfraxAsyncClass.pollset_remove_fd(self->infrax, internal->read_fd);
    
    // Close file descriptors
    close(internal->read_fd);
    close(internal->write_fd);
    
    // Free timer data if it's a timer
    if (event->type == POLYX_EVENT_TIMER && internal->data) {
        g_memory->dealloc(g_memory, internal->data);
    }
    
    // Free the event structure
    g_memory->dealloc(g_memory, internal);
    
    // Remove from events array
    for (size_t i = 0; i < self->event_count; i++) {
        if (self->events[i] == event) {
            if (i < self->event_count - 1) {
                memmove(&self->events[i], &self->events[i + 1], 
                       (self->event_count - i - 1) * sizeof(PolyxEvent*));
            }
            self->event_count--;
            break;
        }
    }
}

// Create timer
static PolyxEvent* polyx_async_create_timer(PolyxAsync* self, PolyxTimerConfig* config) {
    if (!self || !config) return NULL;
    
    PolyxEventInternal* timer = (PolyxEventInternal*)g_memory->alloc(g_memory, sizeof(PolyxEventInternal));
    if (!timer) return NULL;
    
    memset(timer, 0, sizeof(PolyxEventInternal));
    
    // Create pipe for timer communication
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        g_memory->dealloc(g_memory, timer);
        return NULL;
    }
    
    // Set non-blocking mode
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    
    timer->base.type = POLYX_EVENT_TIMER;
    timer->read_fd = pipefd[0];
    timer->write_fd = pipefd[1];
    timer->callback = config->callback;
    timer->arg = config->arg;
    
    // Create timer data
    TimerData* timer_data = (TimerData*)g_memory->alloc(g_memory, sizeof(TimerData));
    if (!timer_data) {
        close(pipefd[0]);
        close(pipefd[1]);
        g_memory->dealloc(g_memory, timer);
        return NULL;
    }
    
    timer_data->interval_ms = config->interval_ms;
    timer_data->next_trigger = g_core->time_monotonic_ms(g_core) + config->interval_ms;
    timer_data->is_periodic = true;
    timer_data->callback = config->callback;
    timer_data->arg = config->arg;
    timer->data = timer_data;
    
    // Add to events array
    if (self->event_count >= self->event_capacity) {
        size_t new_capacity = self->event_capacity * 2;
        PolyxEvent** new_events = (PolyxEvent**)g_memory->alloc(g_memory, sizeof(PolyxEvent*) * new_capacity);
        if (!new_events) {
            g_memory->dealloc(g_memory, timer_data);
            close(pipefd[0]);
            close(pipefd[1]);
            g_memory->dealloc(g_memory, timer);
            return NULL;
        }
        memcpy(new_events, self->events, sizeof(PolyxEvent*) * self->event_count);
        g_memory->dealloc(g_memory, self->events);
        self->events = new_events;
        self->event_capacity = new_capacity;
    }
    
    self->events[self->event_count++] = (PolyxEvent*)timer;
    
    // Add read end to pollset
    if (InfraxAsyncClass.pollset_add_fd(self->infrax, timer->read_fd, INFRAX_POLLIN, timer_callback_wrapper, timer) < 0) {
        self->event_count--;
        g_memory->dealloc(g_memory, timer_data);
        close(pipefd[0]);
        close(pipefd[1]);
        g_memory->dealloc(g_memory, timer);
        return NULL;
    }
    
    return (PolyxEvent*)timer;
}

// Start timer
static void polyx_async_start_timer(PolyxAsync* self, PolyxEvent* timer) {
    if (!self || !timer || timer->type != POLYX_EVENT_TIMER) return;
    
    PolyxEventInternal* internal = (PolyxEventInternal*)timer;
    TimerData* timer_data = (TimerData*)internal->data;
    if (!timer_data) return;
    
    // Update next trigger time
    timer_data->next_trigger = g_core->time_monotonic_ms(g_core) + timer_data->interval_ms;
}

// Stop timer
static void polyx_async_stop_timer(PolyxAsync* self, PolyxEvent* timer) {
    if (!self || !timer || timer->type != POLYX_EVENT_TIMER) return;
    
    PolyxEventInternal* internal = (PolyxEventInternal*)timer;
    TimerData* timer_data = (TimerData*)internal->data;
    if (!timer_data) return;
    
    // Free timer data
    g_memory->dealloc(g_memory, timer_data);
    internal->data = NULL;
}

// Poll events
static int polyx_async_poll(PolyxAsync* self, int timeout_ms) {
    if (!self) return -1;
    
    // Check timers
    int64_t now = g_core->time_monotonic_ms(g_core);
    for (size_t i = 0; i < self->event_count; i++) {
        PolyxEvent* event = self->events[i];
        if (!event) continue;
        
        if (event->type == POLYX_EVENT_TIMER) {
            PolyxEventInternal* internal = (PolyxEventInternal*)event;
            TimerData* timer = (TimerData*)internal->data;
            if (timer && timer->callback && now >= timer->next_trigger) {
                // Write to pipe to trigger callback
                char dummy = 1;
                write(internal->write_fd, &dummy, 1);
            }
            g_core->hint_yield(g_core);
        }
    }
    
    // Process pollset events with minimal timeout
    return InfraxAsyncClass.pollset_poll(self->infrax, timeout_ms);
}

// Global class instance
const PolyxAsyncClassType PolyxAsyncClass = {
    .new = polyx_async_new,
    .free = polyx_async_free
};

// 异步间隔执行回调
void async_interval_fn(InfraxAsync* async, void* arg) {
    if (!async || !arg) return;
    
    IntervalTask* task = (IntervalTask*)arg;
    
    while (task->current < task->count && async->state != INFRAX_ASYNC_REJECTED) {
        // 使用 Core 的 sleep_ms 函数
        g_core->sleep_ms(g_core, task->ms);
        
        task->current++;
        
        // 每次间隔后让出 CPU
        InfraxAsyncClass.yield(async);
    }
    
    async->state = INFRAX_ASYNC_FULFILLED;
}
