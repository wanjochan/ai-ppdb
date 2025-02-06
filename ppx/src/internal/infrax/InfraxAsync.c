#include "InfraxCore.h"
#include "InfraxAsync.h"
#include "InfraxLog.h"

// Coroutine queue structure
typedef struct {
    InfraxAsync* ready;     // Ready queue head
    InfraxAsync* current;   // Currently running coroutine
    jmp_buf env;           // Scheduler context
} InfraxCoroutineQueue;

// Thread-local coroutine queue
static __thread InfraxCoroutineQueue g_coroutine_queue = {
    .ready = NULL,
    .current = NULL,
    .env = {0}
};

// Create new coroutine
static InfraxAsync* async_new(const InfraxAsyncConfig* config) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Creating coroutine");
    
    if (!config || !config->fn || !config->name) {
        log->error(log, "Invalid coroutine configuration");
        return NULL;
    }

    // Allocate coroutine instance
    InfraxAsync* self = malloc(sizeof(InfraxAsync));
    if (!self) {
        log->error(log, "Failed to allocate coroutine");
        return NULL;
    }

    // Initialize fields
    memset(self, 0, sizeof(InfraxAsync));
    self->klass = &InfraxAsync_CLASS;
    self->config = *config;
    self->started = false;
    self->done = false;
    self->next = NULL;

    // Set instance methods
    self->start = async_start;
    self->yield = async_yield;
    self->resume = async_resume;
    self->is_done = async_is_done;
    
    log->debug(log, "Coroutine created successfully");
    return self;
}

// Free coroutine
static void async_free(InfraxAsync* self) {
    if (self) {
        free(self);
    }
}

// Start coroutine
static InfraxError async_start(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Starting coroutine");
    
    if (!self || self->started) {
        return make_error(INFRAX_ERROR_INVALID_COROUTINE, "Invalid coroutine state");
    }

    // Add to ready queue tail
    self->next = NULL;
    if (!g_coroutine_queue.ready) {
        g_coroutine_queue.ready = self;
    } else {
        InfraxAsync* last = g_coroutine_queue.ready;
        while (last->next) {
            last = last->next;
        }
        last->next = self;
    }

    return INFRAX_ERROR_OK_STRUCT;
}

// Yield execution
static InfraxError async_yield(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Yielding coroutine");
    
    if (!self || !self->started || self->done) {
        return make_error(INFRAX_ERROR_INVALID_COROUTINE, "Invalid coroutine state");
    }
    
    // Save current context and jump to scheduler
    if (setjmp(self->env) == 0) {
        // Add current coroutine back to ready queue tail
        self->next = NULL;
        if (!g_coroutine_queue.ready) {
            g_coroutine_queue.ready = self;
        } else {
            InfraxAsync* last = g_coroutine_queue.ready;
            while (last->next) {
                last = last->next;
            }
            last->next = self;
        }
        
        log->debug(log, "Yielding to scheduler");
        g_coroutine_queue.current = NULL;
        longjmp(g_coroutine_queue.env, 1);  // Jump back to scheduler
    }
    
    return INFRAX_ERROR_OK_STRUCT;  // Resumed from yield
}

// Resume coroutine
static InfraxError async_resume(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Resuming coroutine");
    
    if (!self || !self->started || self->done) {
        return make_error(INFRAX_ERROR_INVALID_COROUTINE, "Invalid coroutine state");
    }

    // Add to ready queue tail
    self->next = NULL;
    if (!g_coroutine_queue.ready) {
        g_coroutine_queue.ready = self;
    } else {
        InfraxAsync* last = g_coroutine_queue.ready;
        while (last->next) {
            last = last->next;
        }
        last->next = self;
    }

    return INFRAX_ERROR_OK_STRUCT;
}

// Check if coroutine is done
static bool async_is_done(const InfraxAsync* self) {
    return self ? self->done : true;
}

// Run all coroutines in the ready queue
void InfraxAsyncRun(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Running coroutines");
    
    if (!g_coroutine_queue.ready) {
        log->debug(log, "No ready coroutines");
        return;
    }
    
    // Take out a ready coroutine
    InfraxAsync* co = g_coroutine_queue.ready;
    g_coroutine_queue.ready = co->next;
    co->next = NULL;
    g_coroutine_queue.current = co;
    
    if (!co->started) {
        // If new coroutine, call its function
        log->debug(log, "Starting coroutine function");
        co->started = true;
        co->config.fn(co->config.arg);
        co->done = true;
        log->debug(log, "Coroutine function completed");
        g_coroutine_queue.current = NULL;
    } else if (!co->done) {
        // Resume coroutine context
        log->debug(log, "Resuming coroutine context");
        longjmp(co->env, 1);
    }
}

// The class instance
const InfraxAsyncClass InfraxAsync_CLASS = {
    .new = async_new,
    .free = async_free
};
