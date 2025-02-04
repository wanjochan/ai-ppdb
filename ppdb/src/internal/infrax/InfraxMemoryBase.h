#ifndef INFRAX_MEMORY_BASE_H
#define INFRAX_MEMORY_BASE_H

#include <stddef.h>
#include <stdbool.h>

// Memory statistics
typedef struct {
    size_t total_allocations;    // Total number of allocations
    size_t total_deallocations;  // Total number of deallocations
    size_t current_usage;        // Current memory usage in bytes
    size_t peak_usage;          // Peak memory usage in bytes
} InfraxMemoryStats;

// Memory management modes
typedef enum {
    MEMORY_MODE_BASE,    // Basic memory management
    MEMORY_MODE_POOL,    // Memory pool mode
    MEMORY_MODE_GC       // Garbage collection mode
} InfraxMemoryMode;

// Memory pool configuration
typedef struct {
    size_t initial_size;     // Initial pool size
    size_t alignment;        // Memory alignment
} InfraxMemoryPoolConfig;

// GC configuration
typedef struct {
    size_t heap_size;           // Initial heap size
    size_t collection_threshold; // Memory usage threshold to trigger collection
} InfraxMemoryGcConfig;

// Memory configuration
typedef struct {
    InfraxMemoryMode mode;
    union {
        InfraxMemoryPoolConfig pool_config;
        InfraxMemoryGcConfig gc_config;
    };
} InfraxMemoryConfig;

// Base memory interface
typedef struct InfraxMemoryBase {
    // Memory operations
    void* (*alloc)(struct InfraxMemoryBase*, size_t size);
    void* (*realloc)(struct InfraxMemoryBase*, void* ptr, size_t new_size);
    void (*dealloc)(struct InfraxMemoryBase*, void* ptr);
    void* (*memset)(struct InfraxMemoryBase*, void* ptr, int value, size_t size);
    
    // Configuration
    void (*set_config)(struct InfraxMemoryBase*, const InfraxMemoryConfig* config);
    
    // Stats operations
    void (*get_stats)(struct InfraxMemoryBase*, InfraxMemoryStats* stats);
    void (*reset_stats)(struct InfraxMemoryBase*);

    // Private data
    void* private_data;
} InfraxMemoryBase;

// Constructor and destructor
InfraxMemoryBase* infrax_memory_base_new(void);
void infrax_memory_base_free(InfraxMemoryBase* self);

// Initialize base memory interface
void infrax_memory_base_init(InfraxMemoryBase* base);

#endif // INFRAX_MEMORY_BASE_H
