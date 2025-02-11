#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <setjmp.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

// Function declarations
static InfraxAsync* infrax_async_new(AsyncFunction fn, void* arg);
static void infrax_async_free(InfraxAsync* self);
static InfraxAsync* infrax_async_start(InfraxAsync* self);
static void infrax_async_yield(InfraxAsync* self);
static void infrax_async_set_result(InfraxAsync* self, void* data, size_t size);
static void* infrax_async_get_result(InfraxAsync* self, size_t* size);
static bool infrax_async_is_done(InfraxAsync* self);
static int infrax_async_pollset_add_fd(InfraxAsync* self, int fd, short events, PollCallback cb, void* arg);
static int infrax_async_pollset_remove_fd(InfraxAsync* self, int fd);
static int infrax_async_pollset_poll(InfraxAsync* self, int timeout_ms);
static void infrax_async_cancel(InfraxAsync* self);

// Global class instance
const InfraxAsyncClassType InfraxAsyncClass = {
    .new = infrax_async_new,
    .free = infrax_async_free,
    .start = infrax_async_start,
    .cancel = infrax_async_cancel,
    .set_result = infrax_async_set_result,
    .get_result = infrax_async_get_result,
    .is_done = infrax_async_is_done,
    .pollset_add_fd = infrax_async_pollset_add_fd,
    .pollset_remove_fd = infrax_async_pollset_remove_fd,
    .pollset_poll = infrax_async_pollset_poll,
    .yield = infrax_async_yield
};

// Initialize pollset
static int pollset_init(struct InfraxPollset* ps, size_t initial_capacity) {
    ps->fds = (struct pollfd*)malloc(initial_capacity * sizeof(struct pollfd));
    ps->infos = (struct InfraxPollInfo**)malloc(initial_capacity * sizeof(struct InfraxPollInfo*));
    if (!ps->fds || !ps->infos) {
        free(ps->fds);
        free(ps->infos);
        return -1;
    }
    ps->size = 0;
    ps->capacity = initial_capacity;
    return 0;
}

// Clean up pollset
static void pollset_cleanup(struct InfraxPollset* ps) {
    if (!ps) return;
    for (size_t i = 0; i < ps->size; i++) {
        free(ps->infos[i]);
    }
    free(ps->fds);
    free(ps->infos);
    ps->fds = NULL;
    ps->infos = NULL;
    ps->size = 0;
    ps->capacity = 0;
}

// Add fd to pollset
static int infrax_async_pollset_add_fd(InfraxAsync* self, int fd, short events, PollCallback cb, void* arg) {
    if (!self || !self->ctx || fd < 0) return -1;
    
    struct InfraxAsyncContext* ctx = (struct InfraxAsyncContext*)self->ctx;
    struct InfraxPollset* ps = &ctx->pollset;
    
    // Initialize pollset if needed
    if (ps->capacity == 0) {
        if (pollset_init(ps, 16) < 0) {
            return -1;
        }
    }
    
    // Check if fd already exists
    for (size_t i = 0; i < ps->size; i++) {
        if (ps->fds[i].fd == fd) {
            // Update existing entry
            ps->fds[i].events = events;
            ps->infos[i]->events = events;
            ps->infos[i]->callback = cb;
            ps->infos[i]->arg = arg;
            return 0;
        }
    }
    
    // Grow arrays if needed
    if (ps->size >= ps->capacity) {
        size_t new_capacity = ps->capacity * 2;
        struct pollfd* new_fds = (struct pollfd*)realloc(ps->fds, new_capacity * sizeof(struct pollfd));
        struct InfraxPollInfo** new_infos = (struct InfraxPollInfo**)realloc(ps->infos, new_capacity * sizeof(struct InfraxPollInfo*));
        if (!new_fds || !new_infos) {
            free(new_fds);
            free(new_infos);
            return -1;
        }
        ps->fds = new_fds;
        ps->infos = new_infos;
        ps->capacity = new_capacity;
    }
    
    // Add new entry
    struct InfraxPollInfo* info = (struct InfraxPollInfo*)malloc(sizeof(struct InfraxPollInfo));
    if (!info) return -1;
    
    info->fd = fd;
    info->events = events;
    info->callback = cb;
    info->arg = arg;
    info->next = NULL;
    
    ps->fds[ps->size].fd = fd;
    ps->fds[ps->size].events = events;
    ps->fds[ps->size].revents = 0;
    ps->infos[ps->size] = info;
    ps->size++;
    
    return 0;
}

// Remove fd from pollset
static int infrax_async_pollset_remove_fd(InfraxAsync* self, int fd) {
    if (!self || !self->ctx || fd < 0) return -1;
    
    struct InfraxAsyncContext* ctx = (struct InfraxAsyncContext*)self->ctx;
    struct InfraxPollset* ps = &ctx->pollset;
    
    for (size_t i = 0; i < ps->size; i++) {
        if (ps->fds[i].fd == fd) {
            // Free the info structure
            free(ps->infos[i]);
            
            // Move last element to this position if not last
            if (i < ps->size - 1) {
                ps->fds[i] = ps->fds[ps->size - 1];
                ps->infos[i] = ps->infos[ps->size - 1];
            }
            
            ps->size--;
            return 0;
        }
    }
    
    return -1; // fd not found
}

// Poller
static int infrax_async_pollset_poll(InfraxAsync* self, int timeout_ms) {
    if (!self || !self->ctx) return -1;
    
    struct InfraxAsyncContext* ctx = (struct InfraxAsyncContext*)self->ctx;
    struct InfraxPollset* ps = &ctx->pollset;
    
    if (ps->size == 0) return 0;
    
    // Use poll() to wait for events
    int ret = poll(ps->fds, ps->size, timeout_ms);
    if (ret < 0) return -1;
    
    // Process events
    for (size_t i = 0; i < ps->size; i++) {
        if (ps->fds[i].revents) {
            if (ps->infos[i]->callback) {
                ps->infos[i]->callback(ps->fds[i].fd, ps->fds[i].revents, ps->infos[i]->arg);
            }
            ps->fds[i].revents = 0;  // Clear events
        }
    }
    
    return ret;
}

static InfraxAsync* infrax_async_new(AsyncFunction fn, void* arg) {
    InfraxAsync* self = (InfraxAsync*)malloc(sizeof(InfraxAsync));
    if (!self) return NULL;
    
    self->fn = fn;
    self->arg = arg;
    self->state = INFRAX_ASYNC_PENDING;
    self->ctx = malloc(sizeof(struct InfraxAsyncContext));
    if (!self->ctx) {
        free(self);
        return NULL;
    }
    memset(self->ctx, 0, sizeof(struct InfraxAsyncContext));
    
    // Initialize pollset
    struct InfraxAsyncContext* ctx = (struct InfraxAsyncContext*)self->ctx;
    if (pollset_init(&ctx->pollset, 16) < 0) {
        free(self->ctx);
        free(self);
        return NULL;
    }
    
    self->user_data = NULL;
    self->user_data_size = 0;
    self->next = NULL;
    self->error = 0;
    
    return self;
}

static void infrax_async_free(InfraxAsync* self) {
    if (!self) return;
    if (self->ctx) {
        struct InfraxAsyncContext* ctx = (struct InfraxAsyncContext*)self->ctx;
        if (ctx->stack) {
            free(ctx->stack);
        }
        pollset_cleanup(&ctx->pollset);
        free(ctx);
    }
    if (self->user_data) {
        free(self->user_data);
    }
    free(self);
}

static InfraxAsync* infrax_async_start(InfraxAsync* self) {
    if (!self || !self->fn) return NULL;
    
    struct InfraxAsyncContext* ctx = (struct InfraxAsyncContext*)self->ctx;
    if (!ctx) {
        self->error = ENOMEM;
        self->state = INFRAX_ASYNC_REJECTED;
        return NULL;
    }
    
    // Save current context and start/resume task
    int ret = setjmp(ctx->env);
    if (ret == 0) {
        // First time: start task
        self->fn(self, self->arg);
        // Task completed
        self->state = INFRAX_ASYNC_FULFILLED;
    } else {
        // Resumed from yield
        // Poll for events before continuing
        infrax_async_pollset_poll(self, 0);
    }
    
    return self;
}

static void infrax_async_yield(InfraxAsync* self) {
    if (!self || !self->ctx) return;
    
    struct InfraxAsyncContext* ctx = (struct InfraxAsyncContext*)self->ctx;
    ctx->yield_count++;
    
    // Use longjmp to yield control
    longjmp(ctx->env, 1);
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

static bool infrax_async_is_done(InfraxAsync* self) {
    if (!self) return false;
    return self->state == INFRAX_ASYNC_FULFILLED;
}

static void infrax_async_cancel(InfraxAsync* self) {
    if (!self) return;
    self->state = INFRAX_ASYNC_REJECTED;
} 