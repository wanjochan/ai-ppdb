#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

//design pattern: factory

// #include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Forward declarations
struct InfraxAsync;
struct InfraxTimer;
struct InfraxScheduler;

typedef struct InfraxAsync InfraxAsync;
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

// Instance structure
struct InfraxAsync {
    AsyncFn fn;                     // Async function
    void* arg;                      // Function argument
    InfraxAsyncStatus state;        // Current state
    void* ctx;                      // Internal context (opaque pointer)
    void* user_data;                // User data for async operation，is this result?
    size_t data_size;               // Size of user data
    InfraxAsync* next;              // Next task in ready queue
    int error;                      // Error code
};

// "Class" for static methods
struct InfraxAsyncClassType {
    InfraxAsync* (*new)(AsyncFn fn, void* arg);
    void (*free)(InfraxAsync* self);
    InfraxAsync* (*start)(InfraxAsync* self);
    void (*yield)(InfraxAsync* self);
    void (*set_result)(InfraxAsync* self, void* data, size_t size);
    void* (*get_result)(InfraxAsync* self, size_t* size);
    int (*add_timer)(InfraxAsync* task, int64_t ms, TimerCallback cb, void* arg);
    void (*cancel_timer)(InfraxAsync* task);
    bool (*is_done)(InfraxAsync* self);
};

// Global class instance
extern const struct InfraxAsyncClassType InfraxAsyncClass;

// Scheduler functions
void infrax_scheduler_init(void);
void infrax_scheduler_poll(void);

#endif // INFRAX_ASYNC_H
