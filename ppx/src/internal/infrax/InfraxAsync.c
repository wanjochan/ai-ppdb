#include "internal/infrax/InfraxAsync.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <stdbool.h>

// Forward declarations of internal functions
InfraxAsync* infrax_async_new(AsyncFn fn, void* arg);
void infrax_async_free(InfraxAsync* self);
InfraxAsync* infrax_async_start(InfraxAsync* self, AsyncFn fn, void* arg);
void infrax_async_yield(InfraxAsync* self);
InfraxAsyncStatus infrax_async_status(InfraxAsync* self);
InfraxAsyncResult* infrax_async_wait(InfraxAsync* self, int timeout_ms);
bool infrax_async_poll(InfraxAsync* self, InfraxAsyncResult* result);

// Internal helper function for state change notification
void notify_state_change(InfraxAsync* self) {
    if (!self || !self->result) return;
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    
    char buffer[sizeof(InfraxAsyncStatus)];
    memcpy(buffer, &self->state, sizeof(buffer));
    write(ctx->pipe_fd[1], buffer, sizeof(buffer));
}

// Implementation of instance methods
void infrax_async_yield(InfraxAsync* self) {
    if (!self) return;
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    if (!ctx) return;
    
    ctx->yield_count++;
    self->state = INFRAX_ASYNC_YIELD;
    
    // Notify state change and yield CPU
    notify_state_change(self);
    sched_yield();
    
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
        
        // Initialize pipe
        if (pipe(ctx->pipe_fd) == -1) {
            free(ctx);
            self->state = INFRAX_ASYNC_ERROR;
            self->error = errno;
            return self;
        }
        
        ctx->yield_count = 0;
        ctx->user_data = NULL;
        self->result = ctx;
        
        notify_state_change(self);
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
        notify_state_change(self);
        
        self->fn(self, self->arg);
        
        // If we get here, the function completed without yielding
        self->state = INFRAX_ASYNC_DONE;
        notify_state_change(self);
        
        // Clean up user data if present
        if (ctx->user_data) {
            free(ctx->user_data);
            ctx->user_data = NULL;
        }
        
        // Close pipe and clean up context
        close(ctx->pipe_fd[0]);
        close(ctx->pipe_fd[1]);
        free(ctx);
        self->result = NULL;
    }
    
    return self;
}

InfraxAsyncStatus infrax_async_status(InfraxAsync* self) {
    return self ? self->state : INFRAX_ASYNC_ERROR;
}

InfraxAsyncResult* infrax_async_wait(InfraxAsync* self, int timeout_ms) {
    if (!self || !self->result) return NULL;
    
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    InfraxAsyncResult* result = malloc(sizeof(InfraxAsyncResult));
    if (!result) return NULL;
    
    struct pollfd pfd = {
        .fd = ctx->pipe_fd[0],
        .events = POLLIN,
        .revents = 0
    };
    
    while (timeout_ms != 0) {
        int ret = poll(&pfd, 1, timeout_ms > 0 ? 1 : -1);
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            char buffer[sizeof(InfraxAsyncStatus)];
            read(ctx->pipe_fd[0], buffer, sizeof(buffer));
            
            result->status = self->state;
            result->error_code = self->error;
            result->yield_count = ctx->yield_count;
            
            if (result->status == INFRAX_ASYNC_YIELD) {
                sched_yield();
            } else {
                break;
            }
        } else if (ret < 0) {
            result->status = self->state;
            result->error_code = errno;
            result->yield_count = ctx->yield_count;
            break;
        }
        
        if (timeout_ms > 0) {
            timeout_ms--;
            if (timeout_ms == 0) {
                result->status = self->state;
                result->error_code = 0;
                result->yield_count = ctx->yield_count;
                break;
            }
        }
        
        sched_yield();
    }
    
    return result;
}

bool infrax_async_poll(InfraxAsync* self, InfraxAsyncResult* result) {
    if (!self || !self->result || !result) return false;
    
    InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
    
    struct pollfd pfd = {
        .fd = ctx->pipe_fd[0],
        .events = POLLIN,
        .revents = 0
    };
    
    int ret = poll(&pfd, 1, 0);  // Non-blocking
    if (ret > 0 && (pfd.revents & POLLIN)) {
        char buffer[sizeof(InfraxAsyncStatus)];
        read(ctx->pipe_fd[0], buffer, sizeof(buffer));
        
        result->status = self->state;
        result->error_code = self->error;
        result->yield_count = ctx->yield_count;
        return true;
    }
    
    return false;
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
    self->wait = infrax_async_wait;
    self->poll = infrax_async_poll;
    
    return self;
}

void infrax_async_free(InfraxAsync* self) {
    if (self) {
        if (self->result) {
            InfraxAsyncContext* ctx = (InfraxAsyncContext*)self->result;
            if (ctx->user_data) {
                free(ctx->user_data);
            }
            close(ctx->pipe_fd[0]);
            close(ctx->pipe_fd[1]);
            free(ctx);
        }
        free(self);
    }
}

// Global class instance
const InfraxAsyncClass InfraxAsync_CLASS = {
    .new = infrax_async_new,
    .free = infrax_async_free
};