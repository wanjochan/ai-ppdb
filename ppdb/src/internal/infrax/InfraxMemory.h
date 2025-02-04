#ifndef INFRAX_MEMORY_H
#define INFRAX_MEMORY_H

#include "InfraxMemoryBase.h"
#include "InfraxMemoryPool.h"
#include "InfraxMemoryGc.h"

typedef enum {
    MEMORY_MODE_BASE,    // 基础内存管理
    MEMORY_MODE_POOL,    // 内存池模式
    MEMORY_MODE_GC       // 垃圾回收模式
} InfraxMemoryMode;

typedef struct {
    InfraxMemoryMode mode;
    union {
        InfraxMemoryPoolConfig pool_config;
        InfraxMemoryGcConfig gc_config;
    };
} InfraxMemoryConfig;

typedef struct InfraxMemory {
    // Current mode
    InfraxMemoryMode mode;
    
    // Memory implementations
    union {
        InfraxMemoryBase* base;
        InfraxMemoryPool* pool;
        InfraxMemoryGc* gc;
    };
    
    // Methods
    struct InfraxMemory* (*new)(void);
    void (*free)(struct InfraxMemory*);
    
    // Configuration
    void (*set_config)(struct InfraxMemory*, const InfraxMemoryConfig* config);
    
    // Memory operations (delegates to current implementation)
    void* (*alloc)(struct InfraxMemory*, size_t size);
    void* (*realloc)(struct InfraxMemory*, void* ptr, size_t new_size);
    void (*dealloc)(struct InfraxMemory*, void* ptr);
    void* (*memset)(struct InfraxMemory*, void* ptr, int value, size_t size);
    
    // Stats operations
    void (*get_stats)(struct InfraxMemory*, InfraxMemoryStats* stats);
} InfraxMemory;

// Constructor and destructor
InfraxMemory* infrax_memory_new(void);
void infrax_memory_free(InfraxMemory* self);

#endif // INFRAX_MEMORY_H
