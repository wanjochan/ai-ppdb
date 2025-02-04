#ifndef INFRAX_MEMORY_GC_H
#define INFRAX_MEMORY_GC_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "InfraxMemoryBase.h"

// GC object header
typedef struct GcHeader {
    struct GcHeader* next;  // Next object in the list
    size_t size;           // Size of the user data
    bool marked;           // Mark flag for GC
} GcHeader;

// GC implementation
typedef struct {
    InfraxMemoryBase base;
    InfraxMemoryGcConfig config;
    InfraxMemoryStats stats;
    void* heap_start;
    size_t heap_size;
    void* free_list;
    GcHeader* objects;     // List of allocated objects
    void* stack_bottom;    // Bottom of the stack for root scanning
} InfraxMemoryGc;

// Constructor and destructor
InfraxMemoryGc* infrax_memory_gc_new(void);
void infrax_memory_gc_free(InfraxMemoryGc* self);

// Configuration
bool infrax_memory_gc_set_config(InfraxMemoryGc* self, const InfraxMemoryGcConfig* config);

#endif // INFRAX_MEMORY_GC_H
