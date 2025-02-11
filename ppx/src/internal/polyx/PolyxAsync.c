#include "internal/polyx/PolyxAsync.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxCore.h"
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
extern const InfraxMemoryClassType InfraxMemoryClass;

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

// Timer data structure
typedef struct {
    int64_t interval_ms;    // Timer interval
    int64_t next_trigger;   // Next trigger time
    bool is_periodic;       // Whether timer repeats
    TimerCallback callback; // Timer callback
    void* arg;             // Callback argument
} TimerData;

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
    return g_memory != NULL && g_core != NULL;
}

// Forward declarations of internal functions
static void polyx_async_free(PolyxAsync* self);
static PolyxEvent* polyx_async_create_event(PolyxAsync* self, const PolyxEventConfig* config);
static int polyx_async_trigger_event(PolyxAsync* self, PolyxEvent* event, void* data, size_t size);
static void polyx_async_destroy_event(PolyxAsync* self, PolyxEvent* event);
static PolyxEvent* polyx_async_create_timer(PolyxAsync* self, const PolyxTimerConfig* config);
static int polyx_async_start_timer(PolyxAsync* self, PolyxEvent* timer);
static int polyx_async_stop_timer(PolyxAsync* self, PolyxEvent* timer);
static int polyx_async_poll(PolyxAsync* self, int timeout_ms);

// Timer callback wrapper
static void timer_callback_wrapper(int fd, short events, void* arg) {
    PolyxEvent* timer = (PolyxEvent*)arg;
    if (timer->data) {
        TimerCallback callback = (TimerCallback)timer->data;
        callback(timer->data_size ? (void*)timer->data_size : NULL);
    }
}

// Event callback wrapper
static void event_callback_wrapper(int fd, short events, void* arg) {
    PolyxEvent* event = (PolyxEvent*)arg;
    if (event->data) {
        EventCallback callback = (EventCallback)event->data;
        callback(event, event->data_size ? (void*)event->data_size : NULL);
    }
}

// Create new PolyxAsync instance
static PolyxAsync* polyx_async_new(void) {
    // Initialize memory first
    if (!init_memory()) {
        return NULL;
    }
    
    PolyxAsync* self = (PolyxAsync*)malloc(sizeof(PolyxAsync));
    if (!self) return NULL;
    
    // Initialize context
    PolyxAsyncContext* ctx = (PolyxAsyncContext*)malloc(sizeof(PolyxAsyncContext));
    if (!ctx) {
        free(self);
        return NULL;
    }
    
    // Get memory manager
    ctx->memory = g_memory;
    if (!ctx->memory) {
        free(ctx);
        free(self);
        return NULL;
    }
    
    // Initialize poll fds
    ctx->fds_capacity = 32;
    ctx->fds = (struct pollfd*)malloc(sizeof(struct pollfd) * ctx->fds_capacity);
    if (!ctx->fds) {
        free(ctx);
        free(self);
        return NULL;
    }
    ctx->fds_count = 0;
    
    // Initialize timers
    ctx->timers_capacity = 32;
    ctx->timers = (TimerData**)malloc(sizeof(TimerData*) * ctx->timers_capacity);
    if (!ctx->timers) {
        free(ctx->fds);
        free(ctx);
        free(self);
        return NULL;
    }
    ctx->timers_count = 0;
    
    // Create InfraxAsync instance
    ctx->infra = InfraxAsyncClass.new(NULL, NULL);
    if (!ctx->infra) {
        free(ctx->timers);
        free(ctx->fds);
        free(ctx);
        free(self);
        return NULL;
    }
    
    ctx->private_data = NULL;
    ctx->cleanup_fn = NULL;
    
    self->infra = ctx->infra;
    self->ctx = ctx;
    
    return self;
}

// Free PolyxAsync instance
static void polyx_async_free(PolyxAsync* self) {
    if (!self) return;
    
    PolyxAsyncContext* ctx = (PolyxAsyncContext*)self->ctx;
    if (ctx) {
        if (ctx->cleanup_fn && ctx->private_data) {
            ctx->cleanup_fn(ctx->private_data);
        }
        if (ctx->infra) {
            InfraxAsyncClass.free(ctx->infra);
        }
        if (ctx->fds) {
            free(ctx->fds);
        }
        if (ctx->timers) {
            for (size_t i = 0; i < ctx->timers_count; i++) {
                free(ctx->timers[i]);
            }
            free(ctx->timers);
        }
        free(ctx);
    }
    
    free(self);
}

// Create event
static PolyxEvent* polyx_async_create_event(PolyxAsync* self, const PolyxEventConfig* config) {
    if (!self || !config) return NULL;
    
    PolyxEvent* event = (PolyxEvent*)malloc(sizeof(PolyxEvent));
    if (!event) return NULL;
    
    // Create pipe for event communication
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        free(event);
        return NULL;
    }
    
    // Set non-blocking mode
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    
    event->type = config->type;
    event->read_fd = pipefd[0];
    event->write_fd = pipefd[1];
    event->data = config->callback;
    event->data_size = (size_t)config->arg;
    
    // Add read end to pollset
    if (InfraxAsyncClass.pollset_add_fd(self->infra, event->read_fd, INFRAX_POLLIN, event_callback_wrapper, event) < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        free(event);
        return NULL;
    }
    
    return event;
}

// Trigger event
static int polyx_async_trigger_event(PolyxAsync* self, PolyxEvent* event, void* data, size_t size) {
    if (!self || !event) return -1;
    
    // Write event data to pipe
    ssize_t written = write(event->write_fd, data, size);
    return (written == (ssize_t)size) ? 0 : -1;
}

// Destroy event
static void polyx_async_destroy_event(PolyxAsync* self, PolyxEvent* event) {
    if (!self || !event) return;
    
    InfraxAsyncClass.pollset_remove_fd(self->infra, event->read_fd);
    close(event->read_fd);
    close(event->write_fd);
    free(event);
}

// Create timer
static PolyxEvent* polyx_async_create_timer(PolyxAsync* self, const PolyxTimerConfig* config) {
    if (!self || !config) return NULL;
    
    PolyxEvent* timer = (PolyxEvent*)malloc(sizeof(PolyxEvent));
    if (!timer) return NULL;
    
    PolyxAsyncContext* ctx = (PolyxAsyncContext*)self->ctx;
    
    // Create pipe for timer communication
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        free(timer);
        return NULL;
    }
    
    // Set non-blocking mode
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    
    timer->type = POLYX_EVENT_TIMER;
    timer->read_fd = pipefd[0];
    timer->write_fd = pipefd[1];
    timer->data = config->callback;
    timer->data_size = (size_t)config->arg;
    
    // Create timer data
    TimerData* timer_data = (TimerData*)malloc(sizeof(TimerData));
    if (!timer_data) {
        close(pipefd[0]);
        close(pipefd[1]);
        free(timer);
        return NULL;
    }
    
    timer_data->interval_ms = config->interval_ms;
    timer_data->next_trigger = g_core->time_monotonic_ms(g_core) + config->interval_ms;
    timer_data->is_periodic = true;
    timer_data->callback = config->callback;
    timer_data->arg = config->arg;
    
    // Add to timers array
    if (ctx->timers_count >= ctx->timers_capacity) {
        size_t new_capacity = ctx->timers_capacity * 2;
        TimerData** new_timers = (TimerData**)realloc(ctx->timers, sizeof(TimerData*) * new_capacity);
        if (!new_timers) {
            free(timer_data);
            close(pipefd[0]);
            close(pipefd[1]);
            free(timer);
            return NULL;
        }
        ctx->timers = new_timers;
        ctx->timers_capacity = new_capacity;
    }
    
    ctx->timers[ctx->timers_count++] = timer_data;
    
    // Add read end to pollset
    if (InfraxAsyncClass.pollset_add_fd(self->infra, timer->read_fd, INFRAX_POLLIN, timer_callback_wrapper, timer) < 0) {
        ctx->timers_count--;
        free(timer_data);
        close(pipefd[0]);
        close(pipefd[1]);
        free(timer);
        return NULL;
    }
    
    return timer;
}

// Start timer
static int polyx_async_start_timer(PolyxAsync* self, PolyxEvent* timer) {
    if (!self || !timer || timer->type != POLYX_EVENT_TIMER) return -1;
    
    PolyxAsyncContext* ctx = (PolyxAsyncContext*)self->ctx;
    
    // Find timer data
    TimerData* timer_data = NULL;
    for (size_t i = 0; i < ctx->timers_count; i++) {
        if (ctx->timers[i]->callback == timer->data) {
            timer_data = ctx->timers[i];
            break;
        }
    }
    
    if (!timer_data) return -1;
    
    // Update next trigger time
    timer_data->next_trigger = g_core->time_monotonic_ms(g_core) + timer_data->interval_ms;
    
    return 0;
}

// Stop timer
static int polyx_async_stop_timer(PolyxAsync* self, PolyxEvent* timer) {
    if (!self || !timer || timer->type != POLYX_EVENT_TIMER) return -1;
    
    PolyxAsyncContext* ctx = (PolyxAsyncContext*)self->ctx;
    
    // Find and remove timer data
    for (size_t i = 0; i < ctx->timers_count; i++) {
        if (ctx->timers[i]->callback == timer->data) {
            free(ctx->timers[i]);
            memmove(&ctx->timers[i], &ctx->timers[i + 1], 
                   (ctx->timers_count - i - 1) * sizeof(TimerData*));
            ctx->timers_count--;
            break;
        }
    }
    
    return 0;
}

// Poll events
static int polyx_async_poll(PolyxAsync* self, int timeout_ms) {
    if (!self) return -1;
    
    PolyxAsyncContext* ctx = (PolyxAsyncContext*)self->ctx;
    
    // Check timers
    int64_t now = g_core->time_monotonic_ms(g_core);
    for (size_t i = 0; i < ctx->timers_count; i++) {
        TimerData* timer = ctx->timers[i];
        if (now >= timer->next_trigger) {
            if (timer->callback) {
                timer->callback(timer->arg);
            }
            if (timer->is_periodic) {
                timer->next_trigger = now + timer->interval_ms;
            }
        }
    }
    
    // Process pollset events with minimal timeout
    return InfraxAsyncClass.pollset_poll(self->infra, 1);  // 1ms timeout
}

// Global class instance
const PolyxAsyncClass_t PolyxAsyncClass = {
    .new = polyx_async_new,
    .free = polyx_async_free,
    .create_event = polyx_async_create_event,
    .trigger_event = polyx_async_trigger_event,
    .destroy_event = polyx_async_destroy_event,
    .create_timer = polyx_async_create_timer,
    .start_timer = polyx_async_start_timer,
    .stop_timer = polyx_async_stop_timer,
    .poll = polyx_async_poll
};

// Event callback wrapper for InfraxAsync
static void event_callback(int fd, short revents, void* arg) {
    PolyxEvent* event = (PolyxEvent*)arg;
    if (!event || !event->data) return;
    
    if (revents & INFRAX_POLLIN) {
        char buf[1];
        if (read(fd, buf, 1) > 0) {  // Clear the pipe
            if (event->type == POLYX_EVENT_TIMER) {
                TimerData* timer = (TimerData*)event->data;
                if (timer && timer->callback) {
                    timer->callback(timer->arg);
                    if (timer->is_periodic) {
                        // Update next trigger time
                        timer->next_trigger += timer->interval_ms;
                    }
                }
            }
        }
    }
}

// 异步间隔执行回调
void async_interval_fn(InfraxAsync* async, void* arg) {
    if (!async || !arg) return;
    
    IntervalTask* task = (IntervalTask*)arg;
    
    while (task->current < task->count && async->state != INFRAX_ASYNC_REJECTED) {
        // 使用 Core 的 sleep_ms 函数
        g_core->sleep_ms(g_core, task->ms);
        
        task->current++;
        
        // 每次间隔后让出 CPU
        g_core->yield(g_core);
    }
    
    async->state = INFRAX_ASYNC_FULFILLED;
}
