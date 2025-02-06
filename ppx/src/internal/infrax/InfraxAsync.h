/**
 * @file InfraxAsync.h
 * @brief Async coroutine functionality for the infrax subsystem
 */

#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

#include <setjmp.h>
#include "InfraxCore.h"

// Forward declarations
typedef struct InfraxAsync InfraxAsync;
typedef struct InfraxAsyncClass InfraxAsyncClass;

// Coroutine function type
typedef void (*InfraxAsyncFn)(void* arg);

// Coroutine configuration
typedef struct {
    const char* name;  // Coroutine name
    InfraxAsyncFn fn; // Function to run
    void* arg;        // Function argument
} InfraxAsyncConfig;

// Coroutine state
typedef enum {
    COROUTINE_INIT,    // Initial state
    COROUTINE_READY,   // Ready to run
    COROUTINE_RUNNING, // Currently running
    COROUTINE_YIELDED, // Yielded, waiting to resume
    COROUTINE_DONE     // Finished execution
} CoroutineState;

// Coroutine instance
struct InfraxAsync {
    const InfraxAsyncClass* klass;  // Class pointer
    InfraxAsyncConfig config;       // Configuration
    CoroutineState state;           // Current state
    jmp_buf env;                    // Context
    InfraxAsync* next;              // Next in queue
    
    // Methods
    InfraxError (*start)(InfraxAsync* self);
    InfraxError (*yield)(InfraxAsync* self);
    InfraxError (*resume)(InfraxAsync* self);
    bool (*is_done)(const InfraxAsync* self);
};

// Coroutine class
struct InfraxAsyncClass {
    InfraxAsync* (*new)(const InfraxAsyncConfig* config);
    void (*free)(InfraxAsync* self);
};

// Global class instance
extern const InfraxAsyncClass InfraxAsync_CLASS;

// Run coroutines
void InfraxAsyncRun(void);

#endif // INFRAX_ASYNC_H
