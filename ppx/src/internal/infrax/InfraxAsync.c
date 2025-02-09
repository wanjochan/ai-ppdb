#include "internal/infrax/InfraxAsync.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// Global scheduler instance
static InfraxScheduler g_scheduler = {0};

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
    longjmp(task->ctx->env, 1);
}

InfraxAsync* infrax_async_new(AsyncFn fn, void* arg) {
    InfraxAsync* self = (InfraxAsync*)malloc(sizeof(InfraxAsync));
    if (!self) return NULL;
    
    self->fn = fn;
    self->arg = arg;
    self->state = INFRAX_ASYNC_PENDING;
    self->ctx = NULL;
    self->next = NULL;
    self->error = 0;
    
    return self;
}

void infrax_async_free(InfraxAsync* self) {
    if (!self) return;
    if (self->ctx) {
        if (self->ctx->stack) {
            free(self->ctx->stack);
        }
        if (self->ctx->user_data) {
            free(self->ctx->user_data);
        }
        free(self->ctx);
    }
    free(self);
}

InfraxAsync* infrax_async_start(InfraxAsync* self) {
    if (!self || !self->fn) return NULL;
    
    // Create context if not exists
    if (!self->ctx) {
        self->ctx = (InfraxAsyncContext*)malloc(sizeof(InfraxAsyncContext));
        if (!self->ctx) {
            self->error = ENOMEM;
            self->state = INFRAX_ASYNC_REJECTED;
            return NULL;
        }
        memset(self->ctx, 0, sizeof(InfraxAsyncContext));
    }
    
    // Save current context
    if (setjmp(self->ctx->env) == 0) {
        // Call async function
        self->fn(self, self->arg);
        
        // Mark as done
        self->state = INFRAX_ASYNC_FULFILLED;
    }
    
    return self;
}

void infrax_async_yield(InfraxAsync* self) {
    if (!self || !self->ctx) return;
    
    if (setjmp(self->ctx->env) == 0) {
        self->ctx->yield_count++;
        // Add self to ready queue
        add_to_ready_queue(self);
        // Return to scheduler
        g_scheduler.current = NULL;
        return;
    }
}

void infrax_async_set_result(InfraxAsync* self, void* data, size_t size) {
    if (!self || !self->ctx) return;
    
    if (self->ctx->user_data) {
        free(self->ctx->user_data);
    }
    
    self->ctx->user_data = malloc(size);
    if (self->ctx->user_data) {
        memcpy(self->ctx->user_data, data, size);
        self->ctx->data_size = size;
    }
}

void* infrax_async_get_result(InfraxAsync* self, size_t* size) {
    if (!self || !self->ctx) return NULL;
    if (size) *size = self->ctx->data_size;
    return self->ctx->user_data;
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
int infrax_async_add_timer(InfraxAsync* task, int64_t ms, TimerCallback cb, void* arg) {
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
void infrax_async_cancel_timer(InfraxAsync* task) {
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
bool infrax_async_is_done(InfraxAsync* self) {
    if (!self) return true;  // 空任务视为已完成
    return self->state == INFRAX_ASYNC_FULFILLED || self->state == INFRAX_ASYNC_REJECTED;
}
