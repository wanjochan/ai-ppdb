#ifndef POLYX_ASYNC_H
#define POLYX_ASYNC_H

#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>

// Event types
typedef enum {
    POLYX_EVENT_TIMER,    // Timer event
    POLYX_EVENT_IO,       // I/O event
    POLYX_EVENT_SIGNAL    // Signal event
} PolyxEventType;

// Event structure
typedef struct PolyxEvent {
    PolyxEventType type;   // Event type
    int read_fd;          // Read end of event pipe
    int write_fd;         // Write end of event pipe
    void* data;           // Event data
    size_t data_size;     // Size of event data
} PolyxEvent;

// Forward declarations
typedef struct PolyxAsync PolyxAsync;
typedef void (*TimerCallback)(void* arg);
typedef void (*EventCallback)(PolyxEvent* event, void* arg);

// Timer configuration
typedef struct PolyxTimerConfig {
    int interval_ms;      // Timer interval in milliseconds
    TimerCallback callback; // Timer callback function
    void* arg;            // Callback argument
} PolyxTimerConfig;

// Event configuration
typedef struct PolyxEventConfig {
    PolyxEventType type;   // Event type
    EventCallback callback; // Event callback function
    void* arg;            // Callback argument
} PolyxEventConfig;

// Async structure
struct PolyxAsync {
    InfraxAsync* infra;    // Underlying InfraxAsync instance
    void* ctx;            // Internal context
};

// Async class interface
typedef struct {
    // Lifecycle
    PolyxAsync* (*new)(void);
    void (*free)(PolyxAsync* self);
    
    // Event operations
    PolyxEvent* (*create_event)(PolyxAsync* self, const PolyxEventConfig* config);
    int (*trigger_event)(PolyxAsync* self, PolyxEvent* event, void* data, size_t size);
    void (*destroy_event)(PolyxAsync* self, PolyxEvent* event);
    
    // Timer operations
    PolyxEvent* (*create_timer)(PolyxAsync* self, const PolyxTimerConfig* config);
    int (*start_timer)(PolyxAsync* self, PolyxEvent* timer);
    int (*stop_timer)(PolyxAsync* self, PolyxEvent* timer);
    
    // Poll operations
    int (*poll)(PolyxAsync* self, int timeout_ms);
} PolyxAsyncClass_t;

// Global class instance
extern const PolyxAsyncClass_t PolyxAsyncClass;

#endif // POLYX_ASYNC_H
