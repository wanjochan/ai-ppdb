#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

#include <setjmp.h>
#include <stdbool.h>

// Forward declaration
typedef struct InfraxAsync InfraxAsync;
typedef struct InfraxAsyncContext InfraxAsyncContext;

// Function type for async operations
typedef void (*AsyncFn)(InfraxAsync* self, void* arg);

// Async task states
typedef enum {
    INFRAX_ASYNC_INIT = 0,    // Initial state
    INFRAX_ASYNC_RUNNING,     // Currently running
    INFRAX_ASYNC_YIELD,       // Yielded control
    INFRAX_ASYNC_DONE,        // Completed
    INFRAX_ASYNC_ERROR        // Error occurred
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

// Class interface for InfraxAsync
typedef struct {
    InfraxAsync* (*new)(AsyncFn fn, void* arg);
    void (*free)(InfraxAsync* self);
} InfraxAsyncClass;

// Instance structure
struct InfraxAsync {
    const InfraxAsyncClass* klass;  // Class method table
    InfraxAsync* self;              // Instance pointer
    
    // Internal state
    AsyncFn fn;                     // Async function
    void* arg;                      // Function argument
    InfraxAsyncStatus state;        // Current state
    int error;                      // Error code
    void* result;                   // Operation result

    // Instance methods
    InfraxAsync* (*start)(InfraxAsync* self, AsyncFn fn, void* arg);
    InfraxAsync* (*resume)(InfraxAsync* self);  // Added resume method
    void (*yield)(InfraxAsync* self);
    InfraxAsyncStatus (*status)(InfraxAsync* self);
    // InfraxAsyncResult* (*wait)(InfraxAsync* self, int timeout_ms);
    // bool (*poll)(InfraxAsync* self, InfraxAsyncResult* result);
};

// Global class instance
extern const InfraxAsyncClass InfraxAsync_CLASS;

#endif // INFRAX_ASYNC_H
