#include <stdlib.h>
#include <string.h>
#include "InfraxMemory.h"

// Forward declarations of internal functions
static void memory_free(InfraxMemory* self);
static void memory_set_config(InfraxMemory* self, const InfraxMemoryConfig* config);
static void* memory_alloc(InfraxMemory* self, size_t size);
static void* memory_realloc(InfraxMemory* self, void* ptr, size_t new_size);
static void memory_dealloc(InfraxMemory* self, void* ptr);
static void* memory_memset(InfraxMemory* self, void* ptr, int value, size_t size);
static void memory_get_stats(InfraxMemory* self, InfraxMemoryStats* stats);

// Constructor
InfraxMemory* infrax_memory_new(void) {
    InfraxMemory* self = (InfraxMemory*)malloc(sizeof(InfraxMemory));
    if (!self) return NULL;
    
    // Initialize function pointers
    self->free = memory_free;
    self->set_config = memory_set_config;
    self->alloc = memory_alloc;
    self->realloc = memory_realloc;
    self->dealloc = memory_dealloc;
    self->memset = memory_memset;
    self->get_stats = memory_get_stats;
    
    // Set default mode and implementation
    self->mode = MEMORY_MODE_BASE;
    self->base = NULL;
    
    return self;
}

// Destructor
static void memory_free(InfraxMemory* self) {
    if (!self) return;
    
    // Free current implementation
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            if (self->base) infrax_memory_base_free(self->base);
            break;
        case MEMORY_MODE_POOL:
            if (self->pool) infrax_memory_pool_free(self->pool);
            break;
        case MEMORY_MODE_GC:
            if (self->gc) infrax_memory_gc_free(self->gc);
            break;
    }
    
    free(self);
}

// Configuration
static void memory_set_config(InfraxMemory* self, const InfraxMemoryConfig* config) {
    if (!self || !config) return;
    
    // Clean up old implementation
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            if (self->base) infrax_memory_base_free(self->base);
            break;
        case MEMORY_MODE_POOL:
            if (self->pool) infrax_memory_pool_free(self->pool);
            break;
        case MEMORY_MODE_GC:
            if (self->gc) infrax_memory_gc_free(self->gc);
            break;
    }
    
    // Initialize new implementation
    self->mode = config->mode;
    switch (config->mode) {
        case MEMORY_MODE_BASE:
            self->base = (InfraxMemoryBase*)malloc(sizeof(InfraxMemoryBase));
            if (self->base) {
                infrax_memory_base_init(self->base);
                self->base->set_config = NULL; // 基础模式不需要配置
            }
            break;
            
        case MEMORY_MODE_POOL:
            self->pool = infrax_memory_pool_new();
            if (self->pool) infrax_memory_pool_set_config(self->pool, &config->pool_config);
            break;
            
        case MEMORY_MODE_GC:
            self->gc = infrax_memory_gc_new();
            if (self->gc) infrax_memory_gc_set_config(self->gc, &config->gc_config);
            break;
    }
}

// Memory operations
static void* memory_alloc(InfraxMemory* self, size_t size) {
    if (!self) return NULL;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            return self->base ? self->base->alloc(self->base, size) : NULL;
        case MEMORY_MODE_POOL:
            return self->pool ? self->pool->base.alloc(&self->pool->base, size) : NULL;
        case MEMORY_MODE_GC:
            return self->gc ? self->gc->base.alloc(&self->gc->base, size) : NULL;
        default:
            return NULL;
    }
}

static void* memory_realloc(InfraxMemory* self, void* ptr, size_t new_size) {
    if (!self) return NULL;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            return self->base ? self->base->realloc(self->base, ptr, new_size) : NULL;
        case MEMORY_MODE_POOL:
            return self->pool ? self->pool->base.realloc(&self->pool->base, ptr, new_size) : NULL;
        case MEMORY_MODE_GC:
            return self->gc ? self->gc->base.realloc(&self->gc->base, ptr, new_size) : NULL;
        default:
            return NULL;
    }
}

static void memory_dealloc(InfraxMemory* self, void* ptr) {
    if (!self) return;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            if (self->base) self->base->dealloc(self->base, ptr);
            break;
        case MEMORY_MODE_POOL:
            if (self->pool) self->pool->base.dealloc(&self->pool->base, ptr);
            break;
        case MEMORY_MODE_GC:
            if (self->gc) self->gc->base.dealloc(&self->gc->base, ptr);
            break;
    }
}

static void* memory_memset(InfraxMemory* self, void* ptr, int value, size_t size) {
    if (!self) return NULL;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            return self->base ? self->base->memset(self->base, ptr, value, size) : NULL;
        case MEMORY_MODE_POOL:
            return self->pool ? self->pool->base.memset(&self->pool->base, ptr, value, size) : NULL;
        case MEMORY_MODE_GC:
            return self->gc ? self->gc->base.memset(&self->gc->base, ptr, value, size) : NULL;
        default:
            return NULL;
    }
}

static void memory_get_stats(InfraxMemory* self, InfraxMemoryStats* stats) {
    if (!self || !stats) return;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            if (self->base) self->base->get_stats(self->base, stats);
            break;
        case MEMORY_MODE_POOL:
            if (self->pool) self->pool->base.get_stats(&self->pool->base, stats);
            break;
        case MEMORY_MODE_GC:
            if (self->gc) self->gc->base.get_stats(&self->gc->base, stats);
            break;
        default:
            memset(stats, 0, sizeof(InfraxMemoryStats));
    }
}
