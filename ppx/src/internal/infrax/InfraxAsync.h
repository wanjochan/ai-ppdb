#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

//async with setjmp/longjmp

#include <setjmp.h>
#include <stdbool.h>

// Forward declaration
typedef struct InfraxAsync InfraxAsync;
typedef struct InfraxAsyncContext InfraxAsyncContext;

// Function type for async operations
typedef void (*AsyncFn)(InfraxAsync* self, void* arg);

//我觉得 RUNNING 和 YIELD 有点像，看了代码其实可以合并？

// Async task states
typedef enum {
    // INFRAX_ASYNC_INIT = 0,    // Initial state
    // INFRAX_ASYNC_RUNNING,     // Currently running
    // INFRAX_ASYNC_YIELD,       // Yielded control
    // INFRAX_ASYNC_DONE,        // Completed
    // INFRAX_ASYNC_ERROR,        // Error occurred
    INFRAX_ASYNC_PENDING,    //INIT,RUNNING,YIELD
    INFRAX_ASYNC_FULFILLED,  // from DONE
    INFRAX_ASYNC_REJECTED    // from ERROR
} InfraxAsyncStatus;

// Result structure for async operations
typedef struct {
    InfraxAsyncStatus status;  // Current status
    int error_code;           // Error code if any
    int yield_count;          // Number of yields
} InfraxAsyncResult;

// Internal context structure
struct InfraxAsyncContext {
    jmp_buf env;           // Saved execution context
    int yield_count;       // Number of yields
    void* user_data;       // User data for async operation
    int pipe_fd[2];        // Status notification pipe
};

// Instance structure
struct InfraxAsync {
    // const InfraxAsyncClass* klass;  // Class method table
    InfraxAsync* self;              // Instance pointer
    
    // Internal state
    AsyncFn fn;                     // Async function
    void* arg;                      // Function argument
    InfraxAsyncStatus state;        // Current state
    int error;                      // Error code
    void* result;                   // Operation result

    // Instance methods
    InfraxAsync* (*start)(InfraxAsync* self, AsyncFn fn, void* arg);
    void (*yield)(InfraxAsync* self);
    // InfraxAsyncStatus (*status)(InfraxAsync* self);//TODO 这个准备取消，直接 ->state 访问
};

//NOTES: interal use
InfraxAsync* infrax_async_new(AsyncFn fn, void* arg);
void infrax_async_free(InfraxAsync* self);

// "Class" for static methods(not using 'self'):
static struct InfraxAsyncClassType {
    InfraxAsync* (*new)(AsyncFn fn, void* arg);
    void (*free)(InfraxAsync* self);
} InfraxAsyncClass = {
    .new = infrax_async_new,
    .free = infrax_async_free
};

#endif // INFRAX_ASYNC_H
