#include "InfraxAsync.h"
#include "InfraxLog.h"
#include "InfraxCore.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Timer event data
struct timer_data {
    int timeout_ms;
    InfraxTime start_time;
};

// Coroutine state
typedef enum {
    ASYNC_INIT,      // Initial state
    ASYNC_READY,     // Ready to run
    ASYNC_WAITING,   // Waiting for event
    ASYNC_DONE       // Completed
} AsyncState;

// Forward declarations of static functions
static void scheduler_init(InfraxScheduler* self);
static void scheduler_destroy(InfraxScheduler* self);
static void scheduler_run(InfraxScheduler* self);
static InfraxEventSource* scheduler_create_io_event(InfraxScheduler* self, int fd, int events);
static InfraxEventSource* scheduler_create_timer_event(InfraxScheduler* self, int ms);
static InfraxEventSource* scheduler_create_custom_event(
    InfraxScheduler* self,
    void* source,
    int (*ready)(void* source),
    int (*wait)(void* source),
    void (*cleanup)(void* source)
);

static InfraxScheduler* scheduler_new(void);
static void scheduler_free(InfraxScheduler* self);

static InfraxEventSource* event_source_new(InfraxScheduler* scheduler);
static void event_source_free(InfraxEventSource* self);
static int event_source_is_ready(InfraxEventSource* self);
static int event_source_wait_for(InfraxEventSource* self);

static InfraxAsync* async_new(const InfraxAsyncConfig* config);
static void async_free(InfraxAsync* self);
static int async_wait(InfraxAsync* self, InfraxEventSource* event);
static void async_resume(InfraxAsync* self);
static bool async_is_done(InfraxAsync* self);

// Forward declarations of global variables
static const InfraxSchedulerClass scheduler_class;
static const InfraxEventSourceClass event_source_class;
static const InfraxAsyncClass async_class;

// Thread local default scheduler
static _Thread_local InfraxScheduler* g_scheduler = NULL;

// Scheduler class implementation
static InfraxScheduler* scheduler_new(void) {
    InfraxScheduler* self = (InfraxScheduler*)calloc(1, sizeof(InfraxScheduler));
    if (!self) return NULL;

    self->klass = InfraxSchedulerClass_instance();
    scheduler_init(self);
    return self;
}

static void scheduler_free(InfraxScheduler* self) {
    if (!self) return;
    scheduler_destroy(self);
    free(self);
}

static void scheduler_init(InfraxScheduler* self) {
    if (!self) return;
    memset(self, 0, sizeof(InfraxScheduler));
    
    // Set class instance
    self->klass = &scheduler_class;
    
    // Initialize instance methods
    self->run = scheduler_run;
    self->create_io_event = scheduler_create_io_event;
    self->create_timer_event = scheduler_create_timer_event;
    self->create_custom_event = scheduler_create_custom_event;
}

static void scheduler_destroy(InfraxScheduler* self) {
    if (!self) return;
    
    // Clean up all coroutines in ready queue
    InfraxAsync* co = self->ready;
    while (co) {
        InfraxAsync* next = co->next;
        co->klass->free(co);
        co = next;
    }
    
    // Clean up all coroutines in waiting queue
    co = self->waiting;
    while (co) {
        InfraxAsync* next = co->next;
        co->klass->free(co);
        co = next;
    }
}

// Get the default scheduler for current thread
InfraxScheduler* get_default_scheduler(void) {
    if (!g_scheduler) {
        g_scheduler = scheduler_new();
    }
    return g_scheduler;
}

// Event source class implementation
static InfraxEventSource* event_source_new(InfraxScheduler* scheduler) {
    if (!scheduler) return NULL;
    
    InfraxEventSource* self = (InfraxEventSource*)calloc(1, sizeof(InfraxEventSource));
    if (!self) return NULL;
    
    // Set class instance
    self->klass = &event_source_class;
    self->scheduler = scheduler;
    
    return self;
}

static void event_source_free(InfraxEventSource* self) {
    if (!self) return;
    if (self->cleanup) {
        self->cleanup(self->source);
    }
    free(self);
}

// Event source instance methods
static int event_source_is_ready(InfraxEventSource* self) {
    if (!self || !self->source) return 0;
    
    switch (self->type) {
        case EVENT_TIMER: {
            struct timer_data* timer = (struct timer_data*)self->source;
            InfraxCore* core = get_global_infrax_core();
            InfraxTime current_time = core->time_monotonic_ms(core);
            return (current_time - timer->start_time) >= timer->timeout_ms;
        }
        case EVENT_CUSTOM: {
            int (*ready)(void*) = self->ready;
            return ready ? ready(self->source) : 0;
        }
        default:
            return 0;
    }
}

static int event_source_wait_for(InfraxEventSource* self) {
    if (!self || !self->wait) return -1;
    return self->wait(self->source);
}

// Coroutine class implementation
static InfraxAsync* async_new(const InfraxAsyncConfig* config) {
    if (!config || !config->fn) return NULL;
    
    InfraxAsync* self = (InfraxAsync*)calloc(1, sizeof(InfraxAsync));
    if (!self) return NULL;
    
    // Set class instance
    self->klass = &async_class;
    
    // Set scheduler
    self->scheduler = config->scheduler ? config->scheduler : get_default_scheduler();
    
    // Set coroutine properties
    self->name = config->name;
    self->fn = config->fn;
    self->arg = config->arg;
    self->state = ASYNC_READY;
    
    // Set instance methods
    self->wait = async_wait;
    self->is_done = async_is_done;
    self->resume = async_resume;
    
    // Add to scheduler's ready queue
    self->next = self->scheduler->ready;
    self->scheduler->ready = self;
    
    return self;
}

static void async_free(InfraxAsync* self) {
    if (!self) return;
    if (self->event) {
        self->event->klass->free(self->event);
    }
    free(self);
}

// Coroutine instance methods
static int async_wait(InfraxAsync* self, InfraxEventSource* event) {
    if (!self || !event) return -1;
    
    self->event = event;
    self->state = ASYNC_WAITING;
    
    // Remove from ready queue
    InfraxAsync** ready_prev = &self->scheduler->ready;
    while (*ready_prev) {
        if (*ready_prev == self) {
            *ready_prev = self->next;
            break;
        }
        ready_prev = &(*ready_prev)->next;
    }
    
    // Add to waiting queue
    self->next = self->scheduler->waiting;
    self->scheduler->waiting = self;
    
    return 0;
}

static void async_resume(InfraxAsync* self) {
    if (!self) return;
    
    self->state = ASYNC_READY;
    self->event = NULL;
    
    // Remove from waiting queue
    InfraxAsync** wait_prev = &self->scheduler->waiting;
    while (*wait_prev) {
        if (*wait_prev == self) {
            *wait_prev = self->next;
            break;
        }
        wait_prev = &(*wait_prev)->next;
    }
    
    // Add to ready queue
    self->next = self->scheduler->ready;
    self->scheduler->ready = self;
}

static bool async_is_done(InfraxAsync* self) {
    return !self || self->state == ASYNC_DONE;
}

// Global class instances
static const InfraxSchedulerClass scheduler_class = {
    .new = scheduler_new,
    .free = scheduler_free,
    .init = scheduler_init,
    .destroy = scheduler_destroy
};

static const InfraxEventSourceClass event_source_class = {
    .new = event_source_new,
    .free = event_source_free
};

static const InfraxAsyncClass async_class = {
    .new = async_new,
    .free = async_free
};

const InfraxSchedulerClass* InfraxSchedulerClass_instance(void) {
    return &scheduler_class;
}

const InfraxEventSourceClass* InfraxEventSourceClass_instance(void) {
    return &event_source_class;
}

const InfraxAsyncClass* InfraxAsyncClass_instance(void) {
    return &async_class;
}

// Implementation of scheduler methods
static void scheduler_run(InfraxScheduler* self) {
    if (!self) return;
    self->is_running = true;
    
    while (self->is_running && (self->ready || self->waiting)) {
        // Process ready coroutines
        InfraxAsync** ready_prev = &self->ready;
        while (*ready_prev) {
            InfraxAsync* co = *ready_prev;
            
            // Skip and remove if already done
            if (co->state == ASYNC_DONE) {
                *ready_prev = co->next;
                continue;
            }
            
            // Remove from ready queue
            *ready_prev = co->next;
            co->next = NULL;
            
            // Run coroutine
            self->current = co;
            co->fn(co->arg);
            self->current = NULL;
            
            // Handle coroutine state after execution
            if (co->state == ASYNC_READY) {
                // Add back to ready queue if not waiting
                co->next = self->ready;
                self->ready = co;
            } else if (co->state == ASYNC_WAITING) {
                // Already moved to waiting queue by wait()
                continue;
            } else if (co->state == ASYNC_DONE) {
                // Coroutine explicitly marked as done
                continue;
            }
        }
        
        // Check waiting coroutines
        InfraxAsync** wait_prev = &self->waiting;
        while (*wait_prev) {
            InfraxAsync* co = *wait_prev;
            
            // Skip and remove if done
            if (co->state == ASYNC_DONE) {
                *wait_prev = co->next;
                continue;
            }
            
            // Check if event is ready
            if (co->event && co->event->is_ready(co->event)) {
                // Remove from waiting queue
                *wait_prev = co->next;
                
                // Resume coroutine
                co->resume(co);
            } else {
                wait_prev = &co->next;
            }
        }
        
        // Break if no more work to do
        if (!self->ready && !self->waiting) {
            break;
        }
    }
    
    self->is_running = false;
}

static InfraxEventSource* scheduler_create_io_event(InfraxScheduler* self, int fd, int events) {
    InfraxEventSource* event = InfraxEventSource_new(self);
    if (!event) return NULL;

    event->type = EVENT_IO;
    event->source = malloc(sizeof(int));
    if (!event->source) {
        event->klass->free(event);
        return NULL;
    }
    *(int*)event->source = fd;
    
    // Set instance methods
    event->is_ready = event_source_is_ready;
    event->wait_for = event_source_wait_for;
    
    return event;
}

static InfraxEventSource* scheduler_create_timer_event(InfraxScheduler* self, int ms) {
    InfraxEventSource* event = event_source_new(self);
    if (!event) return NULL;

    event->type = EVENT_TIMER;
    event->source = malloc(sizeof(struct timer_data));
    if (!event->source) {
        event->klass->free(event);
        return NULL;
    }
    
    struct timer_data* timer = (struct timer_data*)event->source;
    timer->timeout_ms = ms;
    InfraxCore* core = get_global_infrax_core();
    timer->start_time = core->time_monotonic_ms(core);
    
    // Set instance methods
    event->is_ready = event_source_is_ready;
    event->wait_for = event_source_wait_for;
    
    return event;
}

static InfraxEventSource* scheduler_create_custom_event(
    InfraxScheduler* self,
    void* source,
    int (*ready)(void* source),
    int (*wait)(void* source),
    void (*cleanup)(void* source)
) {
    InfraxEventSource* event = InfraxEventSource_new(self);
    if (!event) return NULL;

    event->type = EVENT_CUSTOM;
    event->source = source;
    event->ready = ready;
    event->wait = wait;
    event->cleanup = cleanup;
    
    // Set instance methods
    event->is_ready = event_source_is_ready;
    event->wait_for = event_source_wait_for;
    
    return event;
}
