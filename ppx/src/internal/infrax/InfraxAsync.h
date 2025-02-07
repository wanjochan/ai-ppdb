#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

#include <stdbool.h>
#include <stdint.h>

// Forward declarations
typedef struct InfraxScheduler InfraxScheduler;
typedef struct InfraxEventSource InfraxEventSource;
typedef struct InfraxAsync InfraxAsync;
typedef struct InfraxAsyncConfig InfraxAsyncConfig;

// Event types
typedef enum {
    EVENT_IO,
    EVENT_TIMER,
    EVENT_CUSTOM
} EventType;

// Coroutine configuration
struct InfraxAsyncConfig {
    const char* name;
    void (*fn)(void* arg);
    void* arg;
    InfraxScheduler* scheduler;
};

// Scheduler class
typedef struct {
    InfraxScheduler* (*new)(void);
    void (*free)(InfraxScheduler* self);
    void (*init)(InfraxScheduler* self);
    void (*destroy)(InfraxScheduler* self);
} InfraxSchedulerClass;

// Event source class
typedef struct {
    InfraxEventSource* (*new)(InfraxScheduler* scheduler);
    void (*free)(InfraxEventSource* self);
} InfraxEventSourceClass;

// Coroutine class
typedef struct {
    InfraxAsync* (*new)(const InfraxAsyncConfig* config);
    void (*free)(InfraxAsync* self);
} InfraxAsyncClass;

// Get the default scheduler for current thread
InfraxScheduler* get_default_scheduler(void);

// Scheduler instance
struct InfraxScheduler {
    const InfraxSchedulerClass* klass;
    bool is_running;
    InfraxAsync* ready;
    InfraxAsync* waiting;
    InfraxAsync* current;
    
    // Instance methods
    void (*run)(InfraxScheduler* self);
    InfraxEventSource* (*create_io_event)(InfraxScheduler* self, int fd, int events);
    InfraxEventSource* (*create_timer_event)(InfraxScheduler* self, int ms);
    InfraxEventSource* (*create_custom_event)(
        InfraxScheduler* self,
        void* source,
        int (*ready)(void* source),
        int (*wait)(void* source),
        void (*cleanup)(void* source)
    );
};

// Event source instance
struct InfraxEventSource {
    const InfraxEventSourceClass* klass;
    InfraxScheduler* scheduler;
    EventType type;
    void* source;
    
    // Event source callbacks
    int (*ready)(void* source);
    int (*wait)(void* source);
    void (*cleanup)(void* source);
    
    // Instance methods
    int (*is_ready)(InfraxEventSource* self);
    int (*wait_for)(InfraxEventSource* self);
};

// Coroutine instance
struct InfraxAsync {
    const InfraxAsyncClass* klass;
    const char* name;
    void (*fn)(void* arg);
    void* arg;
    int state;
    InfraxScheduler* scheduler;
    InfraxEventSource* event;
    InfraxAsync* next;
    
    // Instance methods
    int (*wait)(InfraxAsync* self, InfraxEventSource* event);
    void (*resume)(InfraxAsync* self);
    bool (*is_done)(InfraxAsync* self);
};

// Global class instances
const InfraxSchedulerClass* InfraxSchedulerClass_instance(void);
const InfraxEventSourceClass* InfraxEventSourceClass_instance(void);
const InfraxAsyncClass* InfraxAsyncClass_instance(void);

// Constructor functions
#define InfraxScheduler_new() (InfraxSchedulerClass_instance()->new())
#define InfraxEventSource_new(scheduler) (InfraxEventSourceClass_instance()->new(scheduler))
#define InfraxAsync_new(config) (InfraxAsyncClass_instance()->new(config))

#endif // INFRAX_ASYNC_H
