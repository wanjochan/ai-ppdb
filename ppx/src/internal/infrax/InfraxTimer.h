#ifndef INFRAX_TIMER_H
#define INFRAX_TIMER_H

#include "internal/infrax/InfraxCore.h"

// Timer callback function type
typedef void (*InfraxTimerCallback)(void* arg);

// Timer structure (opaque)
typedef struct InfraxTimer InfraxTimer;

// Timer class type
typedef struct {
    // Create a new timer
    InfraxError (*new)(InfraxTimer** timer, int timeout_ms, InfraxTimerCallback callback, void* arg);
    
    // Free a timer
    void (*free)(InfraxTimer* timer);
    
    // Start the timer
    InfraxError (*start)(InfraxTimer* timer);
    
    // Stop the timer
    void (*stop)(InfraxTimer* timer);
    
    // Reset the timer with new timeout
    InfraxError (*reset)(InfraxTimer* timer, int timeout_ms);
    
    // Get timer file descriptor
    int (*get_fd)(const InfraxTimer* timer);
    
    // Get next expiration time
    uint64_t (*next_expiration)(void);
    
    // Check if timer is in callback
    InfraxBool (*is_in_callback)(const InfraxTimer* timer);
    
    // Wait for callback to complete
    void (*wait_callback)(const InfraxTimer* timer);
} InfraxTimerClassType;

// Global timer class instance
extern InfraxTimerClassType InfraxTimerClass;

#endif // INFRAX_TIMER_H
