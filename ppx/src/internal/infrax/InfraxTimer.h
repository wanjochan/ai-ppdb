#ifndef INFRAX_TIMER_H
#define INFRAX_TIMER_H

#include "internal/infrax/InfraxCore.h"
#include <time.h>

// Timer callback type
typedef void (*InfraxTimerCallback)(void* arg);

// Timer object
typedef struct InfraxTimer InfraxTimer;

// Timer class structure
typedef struct {
    // Create a new timer with callback
    // timeout_ms: timeout in milliseconds
    // callback: function to call when timer expires
    // arg: argument to pass to callback
    // Returns: InfraxError with code 0 on success
    InfraxError (*new)(InfraxTimer** timer, int timeout_ms, InfraxTimerCallback callback, void* arg);
    
    // Free a timer
    void (*free)(InfraxTimer* timer);
    
    // Start the timer
    InfraxError (*start)(InfraxTimer* timer);
    
    // Stop the timer
    void (*stop)(InfraxTimer* timer);
    
    // Reset the timer with new timeout
    InfraxError (*reset)(InfraxTimer* timer, int timeout_ms);
    
    // Get timer file descriptor for polling
    int (*get_fd)(const InfraxTimer* timer);

    // Get next expiration time in milliseconds
    // Returns UINT64_MAX if no active timers
    uint64_t (*next_expiration)();
} InfraxTimerClassType;

// Global timer class instance
extern InfraxTimerClassType InfraxTimerClass;

// Check and fire expired timers
void infrax_timer_check_expired();

#endif // INFRAX_TIMER_H
