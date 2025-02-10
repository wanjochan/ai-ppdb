#include "internal/infrax/InfraxAsync.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <setjmp.h>

// Internal context structure
typedef struct InfraxAsyncContext {
    jmp_buf env;           // Saved execution context
    void* stack;           // Stack for this coroutine
    size_t stack_size;     // Size of allocated stack
    int yield_count;       // Number of yields for debug
} InfraxAsyncContext;

// Timer structure
struct InfraxTimer {
    int64_t deadline;          // Timeout timestamp
    InfraxAsync* task;         // Associated task
    TimerCallback callback;    // Timeout callback
    void* arg;                 // Callback argument
};

// Scheduler structure
struct InfraxScheduler {
    InfraxAsync* current;      // Currently running task
    InfraxAsync* ready_head;   // Head of ready queue
    InfraxAsync* ready_tail;   // Tail of ready queue
    InfraxTimer* timers;       // Timer array
    size_t timer_count;        // Number of active timers
    size_t timer_capacity;     // Timer array capacity
    int64_t last_poll;         // Last poll timestamp
};

// Function declarations
static InfraxAsync* infrax_async_new(AsyncFn fn, void* arg);
static void infrax_async_free(InfraxAsync* self);
static InfraxAsync* infrax_async_start(InfraxAsync* self);
static void infrax_async_yield(InfraxAsync* self);
static void infrax_async_set_result(InfraxAsync* self, void* data, size_t size);
static void* infrax_async_get_result(InfraxAsync* self, size_t* size);
static int infrax_async_add_timer(InfraxAsync* task, int64_t ms, TimerCallback cb, void* arg);
static void infrax_async_cancel_timer(InfraxAsync* task);
static bool infrax_async_is_done(InfraxAsync* self);

// Global scheduler instance
static InfraxScheduler g_scheduler = {0};

// Global class instance
const struct InfraxAsyncClassType InfraxAsyncClass = {
    .new = infrax_async_new,
    .free = infrax_async_free,
    .start = infrax_async_start,
    .yield = infrax_async_yield,
    .set_result = infrax_async_set_result,
    .get_result = infrax_async_get_result,
    .add_timer = infrax_async_add_timer,
    .cancel_timer = infrax_async_cancel_timer,
    .is_done = infrax_async_is_done
};

// Get current timestamp in milliseconds
static int64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Add task to ready queue
static void add_to_ready_queue(InfraxAsync* task) {
    if (!g_scheduler.ready_head) {
        g_scheduler.ready_head = task;
        g_scheduler.ready_tail = task;
    } else {
        g_scheduler.ready_tail->next = task;
        g_scheduler.ready_tail = task;
    }
    task->next = NULL;
}

// Resume task execution
static void resume_task(InfraxAsync* task) {
    g_scheduler.current = task;
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)task->ctx;
    longjmp(ctx->env, 1);
}

static InfraxAsync* infrax_async_new(AsyncFn fn, void* arg) {
    InfraxAsync* self = (InfraxAsync*)malloc(sizeof(InfraxAsync));
    if (!self) return NULL;
    
    self->fn = fn;
    self->arg = arg;
    self->state = INFRAX_ASYNC_PENDING;
    self->ctx = malloc(sizeof(InfraxAsyncContext));
    if (!self->ctx) {
        free(self);
        return NULL;
    }
    memset(self->ctx, 0, sizeof(InfraxAsyncContext));
    self->user_data = NULL;
    self->data_size = 0;
    self->next = NULL;
    self->error = 0;
    
    return self;
}

static void infrax_async_free(InfraxAsync* self) {
    if (!self) return;
    if (self->ctx) {
        InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->ctx;
        if (ctx->stack) {
            free(ctx->stack);
        }
        free(ctx);
    }
    if (self->user_data) {
        free(self->user_data);
    }
    free(self);
}

static InfraxAsync* infrax_async_start(InfraxAsync* self) {
    if (!self || !self->fn) return NULL;
    
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->ctx;
    if (!ctx) {
        self->error = ENOMEM;
        self->state = INFRAX_ASYNC_REJECTED;
        return NULL;
    }
    
    // Save current context
    if (setjmp(ctx->env) == 0) {
        // Call async function
        self->fn(self, self->arg);
        
        // Mark as done
        self->state = INFRAX_ASYNC_FULFILLED;
    }
    
    return self;
}

static void infrax_async_yield(InfraxAsync* self) {
    if (!self || !self->ctx) return;
    
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->ctx;
    if (setjmp(ctx->env) == 0) {
        ctx->yield_count++;
        // Add self to ready queue
        add_to_ready_queue(self);
        // Return to scheduler
        g_scheduler.current = NULL;
        return;
    }
}

static void infrax_async_set_result(InfraxAsync* self, void* data, size_t size) {
    if (!self) return;
    
    if (self->user_data) {
        free(self->user_data);
    }
    
    if (data && size > 0) {
        self->user_data = malloc(size);
        if (self->user_data) {
            memcpy(self->user_data, data, size);
            self->data_size = size;
        }
    } else {
        self->user_data = NULL;
        self->data_size = 0;
    }
}

static void* infrax_async_get_result(InfraxAsync* self, size_t* size) {
    if (!self) return NULL;
    if (size) *size = self->data_size;
    return self->user_data;
}

// Initialize scheduler
void infrax_scheduler_init(void) {
    g_scheduler.timer_capacity = 16;  // Initial capacity
    g_scheduler.timers = malloc(sizeof(InfraxTimer) * g_scheduler.timer_capacity);
    g_scheduler.timer_count = 0;
    g_scheduler.last_poll = get_timestamp_ms();
    g_scheduler.ready_head = NULL;
    g_scheduler.ready_tail = NULL;
    g_scheduler.current = NULL;
}

// Add a timer
static int infrax_async_add_timer(InfraxAsync* task, int64_t ms, TimerCallback cb, void* arg) {
    if (g_scheduler.timer_count >= g_scheduler.timer_capacity) {
        size_t new_capacity = g_scheduler.timer_capacity * 2;
        InfraxTimer* new_timers = realloc(g_scheduler.timers, 
            sizeof(InfraxTimer) * new_capacity);
        if (!new_timers) {
            return -1;  // Memory allocation failed
        }
        g_scheduler.timers = new_timers;
        g_scheduler.timer_capacity = new_capacity;
    }

    int64_t now = get_timestamp_ms();
    InfraxTimer* timer = &g_scheduler.timers[g_scheduler.timer_count++];
    timer->deadline = now + ms;
    timer->task = task;
    timer->callback = cb;
    timer->arg = arg;
    return 0;
}

// Cancel a timer
static void infrax_async_cancel_timer(InfraxAsync* task) {
    for (size_t i = 0; i < g_scheduler.timer_count; i++) {
        if (g_scheduler.timers[i].task == task) {
            // Move last timer to this slot
            if (i < g_scheduler.timer_count - 1) {
                g_scheduler.timers[i] = g_scheduler.timers[g_scheduler.timer_count - 1];
            }
            g_scheduler.timer_count--;
            break;
        }
    }
}

// Check and trigger expired timers
static void check_timers(void) {
    int64_t now = get_timestamp_ms();
    size_t i = 0;
    
    while (i < g_scheduler.timer_count) {
        InfraxTimer* timer = &g_scheduler.timers[i];
        if (now >= timer->deadline) {
            // Timer expired, trigger callback
            if (timer->callback) {
                timer->callback(timer->arg);
            }
            // Add task to ready queue
            add_to_ready_queue(timer->task);
            // Remove this timer
            if (i < g_scheduler.timer_count - 1) {
                g_scheduler.timers[i] = g_scheduler.timers[g_scheduler.timer_count - 1];
            }
            g_scheduler.timer_count--;
        } else {
            i++;
        }
    }
}

// Poll scheduler
void infrax_scheduler_poll(void) {
    int64_t now = get_timestamp_ms();
    
    // Check timers at least once per second
    if (now >= g_scheduler.last_poll + 1000) {
        check_timers();
        g_scheduler.last_poll = now;
    }
    
    // Process ready queue
    if (g_scheduler.ready_head) {
        InfraxAsync* task = g_scheduler.ready_head;
        g_scheduler.ready_head = task->next;
        if (!g_scheduler.ready_head) {
            g_scheduler.ready_tail = NULL;
        }
        resume_task(task);
    }
}

// 检查异步任务是否完成
static bool infrax_async_is_done(InfraxAsync* self) {
    if (!self) return false;
    return self->state == INFRAX_ASYNC_FULFILLED;
}
