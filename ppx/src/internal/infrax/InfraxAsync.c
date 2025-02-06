#include "InfraxCore.h"
#include "InfraxAsync.h"
#include "InfraxLog.h"

// Error codes
#define INFRAX_ERROR_INVALID_COROUTINE INFRAX_ERROR_INVALID_PARAM

// Coroutine queue structure
typedef struct {
    InfraxAsync* ready;    // Ready queue head
    InfraxAsync* current;  // Currently running coroutine
    jmp_buf env;          // Scheduler context
} InfraxCoroutineQueue;

// Thread-local coroutine queue
static __thread InfraxCoroutineQueue g_queue = {
    .ready = NULL,
    .current = NULL,
    .env = {0}
};

// Forward declarations
static InfraxError async_start(InfraxAsync* self);
static InfraxError async_yield(InfraxAsync* self);
static InfraxError async_resume(InfraxAsync* self);
static bool async_is_done(const InfraxAsync* self);

// Create new coroutine
static InfraxAsync* async_new(const InfraxAsyncConfig* config) {
    InfraxLog* log = get_global_infrax_log();
    if (!config || !config->fn) {
        log->error(log, "Invalid config");
        return NULL;
    }

    InfraxAsync* self = malloc(sizeof(InfraxAsync));
    if (!self) {
        log->error(log, "Failed to allocate coroutine");
        return NULL;
    }

    memset(self, 0, sizeof(InfraxAsync));
    self->klass = &InfraxAsync_CLASS;
    self->config = *config;
    self->state = 0;
    self->next = NULL;

    self->start = async_start;
    self->yield = async_yield;
    self->resume = async_resume;
    self->is_done = async_is_done;
    
    log->debug(log, "Created coroutine %s", config->name);
    return self;
}

// Free coroutine
static void async_free(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    if (self) {
        log->debug(log, "Freeing coroutine %s", self->config.name);
        free(self);
    }
}

// Start coroutine
static InfraxError async_start(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    if (!self || self->state != 0) {
        log->error(log, "Invalid state in start");
        return make_error(INFRAX_ERROR_INVALID_COROUTINE, "Invalid state");
    }

    // Add to ready queue
    self->next = g_queue.ready;
    g_queue.ready = self;
    self->state = 1;
    log->debug(log, "Started coroutine %s", self->config.name);
    return INFRAX_ERROR_OK_STRUCT;
}

// Yield execution
static InfraxError async_yield(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    if (!self || self->state != 2) {
        log->error(log, "Invalid state in yield");
        return make_error(INFRAX_ERROR_INVALID_COROUTINE, "Invalid state");
    }

    // Save current context and switch to scheduler
    log->debug(log, "Yielding coroutine %s", self->config.name);
    if (setjmp(self->env) == 0) {
        self->state = 3;
        g_queue.current = NULL;
        longjmp(g_queue.env, 1);
    }
    self->state = 2;
    log->debug(log, "Resumed coroutine %s", self->config.name);
    return INFRAX_ERROR_OK_STRUCT;
}

// Resume coroutine
static InfraxError async_resume(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    if (!self || self->state != 3) {
        log->error(log, "Invalid state in resume");
        return make_error(INFRAX_ERROR_INVALID_COROUTINE, "Invalid state");
    }

    // Add to ready queue
    self->next = g_queue.ready;
    g_queue.ready = self;
    self->state = 1;
    log->debug(log, "Resumed coroutine %s", self->config.name);
    return INFRAX_ERROR_OK_STRUCT;
}

// Check if coroutine is done
static bool async_is_done(const InfraxAsync* self) {
    return self ? (self->state == 4) : true;
}

// Run coroutines
void InfraxAsyncRun(void) {
    InfraxLog* log = get_global_infrax_log();

    // Return if no ready coroutines
    if (!g_queue.ready) {
        log->debug(log, "No ready coroutines");
        return;
    }

    // Take out a ready coroutine
    InfraxAsync* co = g_queue.ready;
    g_queue.ready = co->next;
    co->next = NULL;
    log->debug(log, "Running coroutine %s", co->config.name);

    // Save scheduler context and run coroutine
    g_queue.current = co;
    co->state = 2;
    int val = setjmp(g_queue.env);
    if (val == 0) {
        if (co->state == 1) {
            // New coroutine: call function directly
            log->debug(log, "Starting coroutine function %s", co->config.name);
            co->config.fn(co->config.arg);
            // If we get here, the coroutine returned without yielding
            log->debug(log, "Coroutine %s returned without yielding", co->config.name);
            co->state = 4;
            g_queue.current = NULL;
        } else {
            // Resume coroutine context
            log->debug(log, "Resuming coroutine %s", co->config.name);
            longjmp(co->env, 1);
        }
    } else {
        // We get here when a coroutine yields
        log->debug(log, "Coroutine %s yielded", co->config.name);
        return;
    }

    // If we get here, the coroutine returned (either directly or after resuming)
    log->debug(log, "Coroutine %s finished", co->config.name);
    co->state = 4;
}

// Class instance
const InfraxAsyncClass InfraxAsync_CLASS = {
    .new = async_new,
    .free = async_free
};
