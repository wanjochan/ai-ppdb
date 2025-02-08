#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

// Forward declaration
typedef struct InfraxAsync InfraxAsync;

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
    
    // Task relationships
    InfraxAsync* next;             // Next task in queue
    InfraxAsync* parent;           // Parent task
    
    // Instance methods
    InfraxAsync* (*start)(InfraxAsync* self, AsyncFn fn, void* arg);
    void (*yield)(InfraxAsync* self);
    InfraxAsyncStatus (*status)(InfraxAsync* self);
};

// Global class instance
extern const InfraxAsyncClass InfraxAsync_CLASS;

#endif // INFRAX_ASYNC_H
