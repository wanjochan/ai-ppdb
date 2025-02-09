#include "internal/infrax/InfraxAsync.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <stdbool.h>

InfraxAsync* infrax_async_new(AsyncFn fn, void* arg);
void infrax_async_free(InfraxAsync* self);
InfraxAsync* infrax_async_start(InfraxAsync* self, AsyncFn fn, void* arg);
void infrax_async_yield(InfraxAsync* self);

// Implementation of instance methods
void infrax_async_yield(InfraxAsync* self) {
    if (!self) return;
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    if (!ctx) return;
    
    ctx->yield_count++;
    self->state = INFRAX_ASYNC_PENDING;
    
    sched_yield();
    
    longjmp(ctx->env, 1);  // Return to saved context
}

InfraxAsync* infrax_async_start(InfraxAsync* self, AsyncFn fn, void* arg) {
    if (!self || !fn) return NULL;
    
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    if (!ctx) {
        self->state = INFRAX_ASYNC_REJECTED;
        return self;
    }
    
    if (setjmp(ctx->env) == 0) {
        self->state = INFRAX_ASYNC_PENDING;
        self->fn(self, self->arg);
        self->state = INFRAX_ASYNC_FULFILLED;
        free(ctx);
        self->result = NULL;
    }
    
    return self;
}

// Implementation of class methods
InfraxAsync* infrax_async_new(AsyncFn fn, void* arg) {
    InfraxAsync* rt = malloc(sizeof(InfraxAsync));
    if (!rt) return NULL;
    
    // Initialize instance
    rt->self = rt;
    rt->fn = fn;
    rt->arg = arg;
    rt->error = 0;
    rt->result = NULL;

    rt->state = INFRAX_ASYNC_PENDING;

    // Create execution context
    InfraxAsyncContext* ctx = malloc(sizeof(InfraxAsyncContext));
    if (!ctx) {
        rt->state = INFRAX_ASYNC_REJECTED;
        rt->error = 1;  // Memory allocation error
        return rt;
    }
    
    ctx->yield_count = 0;//for debug
    rt->result = ctx;

    rt->start = infrax_async_start;
    rt->yield = infrax_async_yield;

    return rt;
}

void infrax_async_free(InfraxAsync* self) {
    if (self) {
        if (self->result) {
            InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
            free(ctx);
        }
        free(self);
    }
}

