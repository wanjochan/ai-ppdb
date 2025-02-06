/**
 * @file InfraxThread.h
 * @brief Thread management functionality for the infrax subsystem
 */

#ifndef INFRAX_THREAD_H
#define INFRAX_THREAD_H

#include <stdbool.h>
#include <pthread.h>
#include "InfraxCore.h"

// Thread ID type
typedef unsigned long InfraxThreadId;

// Error codes
#define INFRAX_ERROR_INVALID_ARGUMENT -1
#define INFRAX_ERROR_THREAD_CREATE_FAILED -2
#define INFRAX_ERROR_THREAD_JOIN_FAILED -3

// Forward declarations
typedef struct InfraxThread InfraxThread;
typedef struct InfraxThreadClass InfraxThreadClass;

// Thread configuration
typedef struct {
    const char* name;
    void* (*entry_point)(void*);
    void* arg;
} InfraxThreadConfig;

// The "static" interface (like static methods in OOP)
struct InfraxThreadClass {
    InfraxThread* (*new)(const InfraxThreadConfig* config);
    void (*free)(InfraxThread* self);
};

// The instance structure
struct InfraxThread {
    const InfraxThreadClass* klass;  // Points to the "class" method table
    
    // Thread data
    InfraxThreadConfig config;
    pthread_t native_handle;
    bool is_running;
    void* result;

    // Instance methods
    InfraxError (*start)(InfraxThread* self);
    InfraxError (*join)(InfraxThread* self, void** result);
    InfraxThreadId (*tid)(InfraxThread* self);
};

// The "static" interface instance
extern const InfraxThreadClass InfraxThread_CLASS;

#endif /* INFRAX_THREAD_H */
