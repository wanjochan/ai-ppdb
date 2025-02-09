#include "internal/infrax/InfraxAsync.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <stdio.h>
// #include <stdbool.h>

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
    
    // Log only every 5000 yields to reduce output
    if (ctx->yield_count % 5000 == 0) {
        printf("Task %p yielded %d times\n", (void*)self, ctx->yield_count);
    }
    
    // 优化的yield策略：
    // 1. 大部分时间使用sched_yield()让出CPU
    // 2. 只在yield次数较多时才考虑短暂休眠
    if (ctx->yield_count > 100000) {
        // 超过10万次yield才考虑休眠
        if (ctx->yield_count % 10000 == 0) {
            usleep(1);  // 最小化休眠时间
        } else {
            sched_yield();
        }
    } else if (ctx->yield_count % 100 == 0) {
        sched_yield();
    }
    
    longjmp(ctx->env, 1);
}

InfraxAsync* infrax_async_start(InfraxAsync* self, AsyncFn fn, void* arg) {
    if (!self) return NULL;
    
    // If fn and arg are provided, update them
    if (fn) self->fn = fn;
    if (arg) self->arg = arg;
    
    // Check if we have valid function
    if (!self->fn) {
        self->state = INFRAX_ASYNC_REJECTED;
        return self;
    }
    
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    if (!ctx) {
        ctx = malloc(sizeof(InfraxAsyncContext));
        if (!ctx) {
            self->state = INFRAX_ASYNC_REJECTED;
            self->error = ENOMEM;  // Set specific error code
            return self;
        }
        ctx->yield_count = 0;
        self->result = ctx;
    }
    
    // Save current context
    if (setjmp(ctx->env) == 0) {
        // First time execution
        self->state = INFRAX_ASYNC_PENDING;
        self->fn(self, self->arg);
        
        // If we reach here and state is still PENDING, task yielded
        return self;
    }
    
    // After yield, continue execution
    self->fn(self, self->arg);
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
        rt->error = ENOMEM;  // Set specific error code
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
