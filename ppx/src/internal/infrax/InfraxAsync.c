#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
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
typedef struct InfraxTimer {
    int64_t deadline;          // Timeout timestamp
    InfraxAsync* task;         // Associated task
    TimerCallback callback;    // Timeout callback
    void* arg;                 // Callback argument
} InfraxTimer;

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
static InfraxAsync* infrax_async_new(AsyncFunction fn, void* arg);
static void infrax_async_free(InfraxAsync* self);
static InfraxAsync* infrax_async_start(InfraxAsync* self);
static void infrax_async_yield(InfraxAsync* self);
static void infrax_async_set_result(InfraxAsync* self, void* data, size_t size);
static void* infrax_async_get_result(InfraxAsync* self, size_t* size);
static int infrax_async_add_timer(InfraxAsync* task, int64_t ms, TimerCallback cb, void* arg);
static void infrax_async_cancel_timer(InfraxAsync* task);
static bool infrax_async_is_done(InfraxAsync* self);

// Global scheduler instance
// static InfraxScheduler g_scheduler = {0};

// Global class instance
const InfraxAsyncClass_t InfraxAsyncClass = {
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

// Simple timer implementation
static int64_t get_timestamp_ms(void) {
    InfraxCore* core = InfraxCoreClass.singleton();
    return core->time_now_ms(core);
}

static void sleep_ms(int64_t ms) {
    InfraxCore* core = InfraxCoreClass.singleton();
    core->sleep_ms(core, ms);
}

// Add a timer
static int infrax_async_add_timer(InfraxAsync* task, int64_t ms, TimerCallback cb, void* arg) {
    if (!task) return -1;
    
    int64_t deadline = get_timestamp_ms() + ms;
    
    while (get_timestamp_ms() < deadline) {
        sleep_ms(10); // Sleep in small intervals
        if (task->state == INFRAX_ASYNC_REJECTED) {
            return -1;
        }
    }
    
    if (cb) {
        cb(arg);
    }
    
    return 0;
}

static InfraxAsync* infrax_async_new(AsyncFunction fn, void* arg) {
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
    self->user_data_size = 0;
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
    ctx->yield_count++;
    
    // Simple yield implementation - just sleep a bit
    InfraxCore* core = InfraxCoreClass.singleton();
    core->yield(core);
    core->sleep_ms(core, 1);  // Small sleep to prevent busy waiting
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
            self->user_data_size = size;
        }
    } else {
        self->user_data = NULL;
        self->user_data_size = 0;
    }
}

static void* infrax_async_get_result(InfraxAsync* self, size_t* size) {
    if (!self) return NULL;
    if (size) *size = self->user_data_size;
    return self->user_data;
}

// Cancel a timer
static void infrax_async_cancel_timer(InfraxAsync* task) {
    if (!task) return;
    task->state = INFRAX_ASYNC_REJECTED;
}

// 检查异步任务是否完成
static bool infrax_async_is_done(InfraxAsync* self) {
    if (!self) return false;
    return self->state == INFRAX_ASYNC_FULFILLED;
}
