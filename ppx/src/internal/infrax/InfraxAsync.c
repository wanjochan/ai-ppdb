#include "internal/infrax/InfraxAsync.h"
#include <stdlib.h>
#include <setjmp.h>
#include <unistd.h>

// Internal structure extension
typedef struct {
    jmp_buf env;           // Saved execution context
    int yield_count;       // Number of yields
    int is_running;        // Task execution state
} InfraxAsyncContext;

// Forward declarations of internal functions
static InfraxAsync* infrax_async_new(AsyncFn fn, void* arg);
static void infrax_async_free(InfraxAsync* self);
static InfraxAsync* infrax_async_start(InfraxAsync* self, AsyncFn fn, void* arg);
static void infrax_async_yield(InfraxAsync* self);
static InfraxAsyncStatus infrax_async_status(InfraxAsync* self);

// Implementation of instance methods
void infrax_async_yield(InfraxAsync* self) {
    if (!self) return;
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    if (!ctx) return;
    
    ctx->yield_count++;
    self->state = INFRAX_ASYNC_YIELD;
    longjmp(ctx->env, 1);
}

InfraxAsync* infrax_async_start(InfraxAsync* self, AsyncFn fn, void* arg) {
    if (!self || !fn) return NULL;
    
    // Initialize task
    self->fn = fn;
    self->arg = arg;
    self->error = 0;
    self->state = INFRAX_ASYNC_INIT;
    self->next = NULL;
    self->parent = self;  // Root task points to itself
    
    // Create execution context
    InfraxAsyncContext* ctx = malloc(sizeof(InfraxAsyncContext));
    if (!ctx) {
        self->state = INFRAX_ASYNC_ERROR;
        self->error = 1;  // Memory allocation error
        return self;
    }
    ctx->yield_count = 0;
    ctx->is_running = 0;
    self->result = ctx;
    
    // Execute task
    if (setjmp(ctx->env) == 0) {
        // First entry
        ctx->is_running = 1;
        self->state = INFRAX_ASYNC_RUNNING;
        self->fn(self, self->arg);  // Pass self to async function
        self->state = INFRAX_ASYNC_DONE;
    } else if (ctx->is_running) {
        // Resume from yield
        self->state = INFRAX_ASYNC_RUNNING;
        usleep(1000);  // Small delay to prevent CPU overload
    }
    
    // Cleanup if done
    if (self->state == INFRAX_ASYNC_DONE) {
        free(ctx);
        self->result = NULL;
    }
    
    return self;
}

InfraxAsyncStatus infrax_async_status(InfraxAsync* self) {
    return self ? self->state : INFRAX_ASYNC_ERROR;
}

// Implementation of class methods
InfraxAsync* infrax_async_new(AsyncFn fn, void* arg) {
    InfraxAsync* self = malloc(sizeof(InfraxAsync));
    if (!self) return NULL;
    
    // Initialize instance
    self->klass = &InfraxAsync_CLASS;
    self->self = self;
    self->fn = fn;
    self->arg = arg;
    self->state = INFRAX_ASYNC_INIT;
    self->error = 0;
    self->result = NULL;
    self->next = NULL;
    self->parent = NULL;
    
    // Set instance methods
    self->start = infrax_async_start;
    self->yield = infrax_async_yield;
    self->status = infrax_async_status;
    
    return self;
}

void infrax_async_free(InfraxAsync* self) {
    if (self) {
        if (self->result) {
            free(self->result);
        }
        free(self);
    }
}

// Global class instance
const InfraxAsyncClass InfraxAsync_CLASS = {
    .new = infrax_async_new,
    .free = infrax_async_free
};