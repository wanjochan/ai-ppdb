#include "internal/infrax/InfraxAsync.h"
#include <stdlib.h>
#include <unistd.h>

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
    longjmp(ctx->env, 1);  // Return to saved context
}

InfraxAsync* infrax_async_start(InfraxAsync* self, AsyncFn fn, void* arg) {
    if (!self || !fn) return NULL;
    
    // Initialize task if not already initialized
    if (self->state == INFRAX_ASYNC_INIT) {
        self->fn = fn;
        self->arg = arg;
        self->error = 0;
        
        // Create execution context
        InfraxAsyncContext* ctx = malloc(sizeof(InfraxAsyncContext));
        if (!ctx) {
            self->state = INFRAX_ASYNC_ERROR;
            self->error = 1;  // Memory allocation error
            return self;
        }
        ctx->yield_count = 0;
        ctx->user_data = NULL;
        self->result = ctx;
    }
    
    // Execute or resume task
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    if (!ctx) {
        self->state = INFRAX_ASYNC_ERROR;
        return self;
    }
    
    if (setjmp(ctx->env) == 0) {
        // First entry or resume from yield
        self->state = INFRAX_ASYNC_RUNNING;
        self->fn(self, self->arg);
        
        // If we get here, the function completed without yielding
        self->state = INFRAX_ASYNC_DONE;
        
        // Clean up user data if present
        if (ctx->user_data) {
            free(ctx->user_data);
            ctx->user_data = NULL;
        }
        
        // Clean up context
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