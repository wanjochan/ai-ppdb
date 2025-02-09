#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

//design pattern: factory
//with setjmp/longjmp

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Forward declarations
struct InfraxAsync;
struct InfraxAsyncContext;
struct InfraxTimer;
struct InfraxScheduler;

typedef struct InfraxAsync InfraxAsync;
typedef struct InfraxAsyncContext InfraxAsyncContext;
typedef struct InfraxTimer InfraxTimer;
typedef struct InfraxScheduler InfraxScheduler;
typedef void (*AsyncFn)(InfraxAsync* self, void* arg);
typedef void (*TimerCallback)(void* arg);

// Async task states
typedef enum {
    INFRAX_ASYNC_PENDING,    //INIT,RUNNING,YIELD
    INFRAX_ASYNC_FULFILLED,  // from DONE
    INFRAX_ASYNC_REJECTED    // from ERROR
} InfraxAsyncStatus;

// Timer structure
struct InfraxTimer {
    int64_t deadline;          // Timeout timestamp
    InfraxAsync* task;         // Associated task
    TimerCallback callback;    // Timeout callback
    void* arg;                 // Callback argument
};

// Scheduler structure
struct InfraxScheduler {
    InfraxAsync* current;      // Currently running task
    InfraxAsync* ready_head;   // Head of ready queue
    InfraxAsync* ready_tail;   // Tail of ready queue
    InfraxTimer* timers;       // Timer array
    size_t timer_count;        // Number of active timers
    size_t timer_capacity;     // Timer array capacity
    int64_t last_poll;         // Last poll timestamp
};

// Internal context structure
struct InfraxAsyncContext {
    jmp_buf env;           // Saved execution context
    void* stack;           // Stack for this coroutine
    size_t stack_size;     // Size of allocated stack
    int yield_count;       // Number of yields for debug
    void* user_data;       // User data for async operation
    size_t data_size;      // Size of user data
};

// Instance structure
struct InfraxAsync {
    AsyncFn fn;                     // Async function
    void* arg;                      // Function argument
    InfraxAsyncStatus state;        // Current state
    InfraxAsyncContext* ctx;        // Execution context
    InfraxAsync* next;             // Next task in ready queue
    int error;                      // Error code
};

// Internal functions
InfraxAsync* infrax_async_new(AsyncFn fn, void* arg);
void infrax_async_free(InfraxAsync* self);
InfraxAsync* infrax_async_start(InfraxAsync* self);
void infrax_async_yield(InfraxAsync* self);
void infrax_async_set_result(InfraxAsync* self, void* data, size_t size);
void* infrax_async_get_result(InfraxAsync* self, size_t* size);
int infrax_async_add_timer(InfraxAsync* task, int64_t ms, TimerCallback cb, void* arg);
void infrax_async_cancel_timer(InfraxAsync* task);
bool infrax_async_is_done(InfraxAsync* self);

// "Class" for static methods
static struct InfraxAsyncClassType {
    InfraxAsync* (*new)(AsyncFn fn, void* arg);
    void (*free)(InfraxAsync* self);
    InfraxAsync* (*start)(InfraxAsync* self);
    void (*yield)(InfraxAsync* self);
    void (*set_result)(InfraxAsync* self, void* data, size_t size);
    void* (*get_result)(InfraxAsync* self, size_t* size);
    int (*add_timer)(InfraxAsync* task, int64_t ms, TimerCallback cb, void* arg);
    void (*cancel_timer)(InfraxAsync* task);
    bool (*is_done)(InfraxAsync* self);
} InfraxAsyncClass = {
    .new = infrax_async_new,
    .free = infrax_async_free,
    .start = infrax_async_start,
    .yield = infrax_async_yield,
    .set_result = infrax_async_set_result,
    .get_result = infrax_async_get_result,
    .add_timer = infrax_async_add_timer,
    .cancel_timer = infrax_async_cancel_timer,
    .is_done = infrax_async_is_done
};

// Scheduler functions
InfraxScheduler* infrax_scheduler_get(void);
void infrax_scheduler_init(void);
void infrax_scheduler_poll(void);

#endif // INFRAX_ASYNC_H
