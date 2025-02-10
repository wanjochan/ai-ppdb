#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

//design pattern: factory

// #include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Forward declarations
typedef struct InfraxAsync InfraxAsync;
typedef void (*AsyncFunction)(InfraxAsync* self, void* arg);
typedef void (*TimerCallback)(void* arg);

// Async task states
typedef enum {
    INFRAX_ASYNC_PENDING,
    INFRAX_ASYNC_FULFILLED,
    INFRAX_ASYNC_REJECTED
} InfraxAsyncState;

// Async task structure
struct InfraxAsync {
    AsyncFunction fn;          // Task function
    void* arg;                // Function argument
    void* ctx;                // Execution context
    void* user_data;          // User data storage
    size_t user_data_size;    // Size of user data
    int error;                // Error code
    InfraxAsyncState state;   // Task state
    InfraxAsync* next;        // Next task in queue
};

// Async class interface
typedef struct {
    // Task management
    InfraxAsync* (*new)(AsyncFunction fn, void* arg);
    void (*free)(InfraxAsync* self);
    InfraxAsync* (*start)(InfraxAsync* self);
    void (*cancel)(InfraxAsync* self);
    void (*yield)(InfraxAsync* self);
    
    // Result handling
    void (*set_result)(InfraxAsync* self, void* data, size_t size);
    void* (*get_result)(InfraxAsync* self, size_t* size);
    
    // Timer operations
    int (*add_timer)(InfraxAsync* self, int64_t ms, TimerCallback cb, void* arg);
    void (*cancel_timer)(InfraxAsync* self);
    
    // State checking
    bool (*is_done)(InfraxAsync* self);
} InfraxAsyncClass_t;

// Global class instance
extern const InfraxAsyncClass_t InfraxAsyncClass;

#endif // INFRAX_ASYNC_H
