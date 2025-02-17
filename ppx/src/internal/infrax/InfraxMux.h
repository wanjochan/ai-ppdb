#ifndef PPDB_INFRAX_MUX_H
#define PPDB_INFRAX_MUX_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxTimer.h"

// Event callback type
typedef void (*InfraxMuxHandler)(int fd, short events, void* arg);

// Class interface
typedef struct InfraxMuxClassType {
    // Timer operations
    InfraxU32 (*setTimeout)(InfraxU32 interval_ms, InfraxMuxHandler handler, void* arg);
    InfraxError (*clearTimeout)(InfraxU32 timer_id);
    
    // Poll events with handler
    InfraxError (*pollall)(const int* fds, size_t nfds, InfraxMuxHandler handler, void* arg, int timeout_ms);
} InfraxMuxClassType;

// Global class instance
extern InfraxMuxClassType InfraxMuxClass;

#endif // PPDB_INFRAX_MUX_H
