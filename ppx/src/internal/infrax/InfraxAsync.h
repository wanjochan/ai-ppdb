/**
 * @file InfraxAsync.h
 * @brief Async coroutine functionality for the infrax subsystem
 */

#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

#include "InfraxCore.h"
#include <setjmp.h>

// Error codes
#define INFRAX_ERROR_INVALID_COROUTINE -1
#define INFRAX_ERROR_COROUTINE_CREATE_FAILED -2
#define INFRAX_ERROR_COROUTINE_YIELD_FAILED -3

// Forward declarations
typedef struct InfraxAsync InfraxAsync;
typedef struct InfraxAsyncClass InfraxAsyncClass;

// Coroutine function type
typedef void (*InfraxAsyncFn)(void* arg);

// Coroutine configuration
typedef struct {
    const char* name;          // Coroutine name
    InfraxAsyncFn fn;         // Function to run
    void* arg;                // Function argument
} InfraxAsyncConfig;

// The "static" interface (like static methods in OOP)
struct InfraxAsyncClass {
    InfraxAsync* (*new)(const InfraxAsyncConfig* config);
    void (*free)(InfraxAsync* self);
};

// The instance structure
struct InfraxAsync {
    const InfraxAsyncClass* klass;  // Points to the "class" method table
    
    // Coroutine data
    InfraxAsyncConfig config;
    jmp_buf env;              // Context for switching
    InfraxAsync* next;        // Next in queue
    bool started;             // Whether started
    bool done;               // Whether completed

    // Instance methods
    InfraxError (*start)(InfraxAsync* self);
    InfraxError (*yield)(InfraxAsync* self);
    InfraxError (*resume)(InfraxAsync* self);
    bool (*is_done)(const InfraxAsync* self);
};

// The "static" interface instance
extern const InfraxAsyncClass InfraxAsync_CLASS;

// Run coroutines
void InfraxAsyncRun(void);

#endif /* INFRAX_ASYNC_H */
