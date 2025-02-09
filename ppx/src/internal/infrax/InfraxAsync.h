#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

//design pattern: factory
//with setjmp/longjmp

#include <setjmp.h>
// #include <stdbool.h>

// Forward declaration
typedef struct InfraxAsync InfraxAsync;
typedef struct InfraxAsyncContext InfraxAsyncContext;

// Function type for async operations
typedef void (*AsyncFn)(InfraxAsync* self, void* arg);

// Async task states
typedef enum {
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
    int yield_count;       // Number of yields for debug
    void* user_data;       // User data for async operation
    void* fn_state;        // Function state for resuming after yield
};

// Instance structure
struct InfraxAsync {
    // const InfraxAsyncClass* klass;  // Class method table
    InfraxAsync* self;              // Instance pointer
    
    // Internal state
    AsyncFn fn;                     // Async function
    void* arg;                      // Function argument
    InfraxAsyncStatus state;        // Current state
    void* result;                   // Operation result

    //TODO change to InfraxError later
    int error;                      // Error code
    char message[128];              // Error message

    // Instance methods
    InfraxAsync* (*start)(InfraxAsync* self, AsyncFn fn, void* arg);
    void (*yield)(InfraxAsync* self);
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
