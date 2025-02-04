#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "InfraxMemoryGc.h"

// Forward declarations
static void* gc_alloc(InfraxMemoryBase* base, size_t size);
static void* gc_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size);
static void gc_dealloc(InfraxMemoryBase* base, void* ptr);
static void* gc_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size);
static void gc_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats);
static void gc_reset_stats(InfraxMemoryBase* base);
static void gc_set_config(InfraxMemoryBase* base, const InfraxMemoryConfig* config);

// Private functions
static GcHeader* get_header(void* ptr);
static void* get_user_ptr(GcHeader* header);
static bool is_pointer_valid(InfraxMemoryGc* self, void* ptr);
static bool should_trigger_gc(InfraxMemoryGc* self);
static void scan_stack(InfraxMemoryGc* self);
static void mark_object(InfraxMemoryGc* self, void* ptr);
static void sweep_phase(InfraxMemoryGc* self);

// Public configuration function
bool infrax_memory_gc_set_config(InfraxMemoryGc* self, const InfraxMemoryGcConfig* config) {
    if (!self || !config) return false;
    
    // Copy configuration
    memcpy(&self->config, config, sizeof(InfraxMemoryGcConfig));
    
    // Ensure valid configuration
    if (self->config.heap_size == 0) {
        self->config.heap_size = 1024 * 1024; // 1MB default
    }
    if (self->config.collection_threshold == 0) {
        self->config.collection_threshold = self->config.heap_size / 2;
    }
    
    // Free old heap if exists
    if (self->heap_start) {
        free(self->heap_start);
        self->heap_start = NULL;
        self->heap_size = 0;
        self->free_list = NULL;
    }
    
    // Allocate new heap
    self->heap_start = malloc(self->config.heap_size);
    if (!self->heap_start) return false;
    
    self->heap_size = self->config.heap_size;
    self->free_list = self->heap_start;
    
    // Reset statistics
    memset(&self->stats, 0, sizeof(InfraxMemoryStats));
    
    return true;
}

// Internal configuration function
static void gc_set_config(InfraxMemoryBase* base, const InfraxMemoryConfig* config) {
    if (!base || !config) return;
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    infrax_memory_gc_set_config(self, &config->gc_config);
}

// Constructor
InfraxMemoryGc* infrax_memory_gc_new(void) {
    InfraxMemoryGc* self = (InfraxMemoryGc*)malloc(sizeof(InfraxMemoryGc));
    if (!self) return NULL;
    
    // Initialize base memory interface
    self->base.alloc = gc_alloc;
    self->base.realloc = gc_realloc;
    self->base.dealloc = gc_dealloc;
    self->base.memset = gc_memset;
    self->base.get_stats = gc_get_stats;
    self->base.reset_stats = gc_reset_stats;
    self->base.set_config = gc_set_config;
    
    // Initialize GC members
    self->heap_start = NULL;
    self->heap_size = 0;
    self->free_list = NULL;
    self->objects = NULL;
    self->stack_bottom = NULL;
    memset(&self->stats, 0, sizeof(InfraxMemoryStats));
    
    return self;
}

// Destructor
void infrax_memory_gc_free(InfraxMemoryGc* self) {
    if (!self) return;
    
    // Free all allocated objects
    GcHeader* current = self->objects;
    while (current) {
        GcHeader* next = current->next;
        free(current);
        current = next;
    }
    
    // Free the heap
    if (self->heap_start) {
        free(self->heap_start);
    }
    
    free(self);
}

// Memory operations
static void* gc_alloc(InfraxMemoryBase* base, size_t size) {
    if (!base || size == 0) return NULL;
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    
    // Check if GC should be triggered
    if (should_trigger_gc(self)) {
        uint64_t start_time = (uint64_t)time(NULL) * 1000;
        scan_stack(self);
        sweep_phase(self);
    }
    
    // Allocate new object
    GcHeader* header = (GcHeader*)malloc(sizeof(GcHeader) + size);
    if (!header) return NULL;
    
    // Initialize header
    header->size = size;
    header->marked = false;
    header->next = self->objects;
    self->objects = header;
    
    // Update statistics
    self->stats.current_usage += size;
    self->stats.total_allocations++;
    if (self->stats.current_usage > self->stats.peak_usage) {
        self->stats.peak_usage = self->stats.current_usage;
    }
    
    return get_user_ptr(header);
}

static void* gc_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size) {
    if (!base) return NULL;
    if (!ptr) return gc_alloc(base, new_size);
    if (new_size == 0) {
        gc_dealloc(base, ptr);
        return NULL;
    }
    
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    GcHeader* header = get_header(ptr);
    if (new_size <= header->size) return ptr;
    
    // Allocate new block
    void* new_ptr = gc_alloc(base, new_size);
    if (!new_ptr) return NULL;
    
    // Copy data and free old block
    memcpy(new_ptr, ptr, header->size);
    gc_dealloc(base, ptr);
    
    return new_ptr;
}

static void gc_dealloc(InfraxMemoryBase* base, void* ptr) {
    if (!base || !ptr) return;
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    
    GcHeader* header = get_header(ptr);
    if (!header) return;
    
    // Update statistics
    self->stats.current_usage -= header->size;
    self->stats.total_deallocations++;
    
    // Mark as free
    header->marked = false;
}

static void* gc_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size) {
    if (!base || !ptr) return NULL;
    return memset(ptr, value, size);
}

static void gc_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats) {
    if (!base || !stats) return;
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    *stats = self->stats;
}

static void gc_reset_stats(InfraxMemoryBase* base) {
    if (!base) return;
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    memset(&self->stats, 0, sizeof(InfraxMemoryStats));
}

// GC implementation
static bool should_trigger_gc(InfraxMemoryGc* self) {
    if (!self) return false;
    return self->stats.current_usage >= self->config.collection_threshold;
}

static void scan_stack(InfraxMemoryGc* self) {
    if (!self || !self->stack_bottom) return;
    
    // Get current stack pointer
    void* stack_top = &stack_top;
    
    // Scan stack for pointers
    void* start = (stack_top < self->stack_bottom) ? stack_top : self->stack_bottom;
    void* end = (stack_top < self->stack_bottom) ? self->stack_bottom : stack_top;
    
    for (void** p = start; p < (void**)end; p++) {
        if (is_pointer_valid(self, *p)) {
            mark_object(self, *p);
        }
    }
}

static void mark_object(InfraxMemoryGc* self, void* ptr) {
    if (!self || !ptr) return;
    
    GcHeader* header = get_header(ptr);
    if (!header || header->marked) return;
    
    // Mark the object
    header->marked = true;
    
    // Scan object's memory for more pointers
    for (void** p = ptr; p < (void**)((char*)ptr + header->size); p++) {
        if (is_pointer_valid(self, *p)) {
            mark_object(self, *p);
        }
    }
}

static void sweep_phase(InfraxMemoryGc* self) {
    if (!self) return;
    
    GcHeader** current = &self->objects;
    while (*current) {
        GcHeader* header = *current;
        
        if (!header->marked) {
            // Remove from list and free
            *current = header->next;
            self->stats.current_usage -= header->size;
            free(header);
        } else {
            // Clear mark for next collection
            header->marked = false;
            current = &header->next;
        }
    }
}

// Helper functions
static GcHeader* get_header(void* ptr) {
    if (!ptr) return NULL;
    return (GcHeader*)((char*)ptr - sizeof(GcHeader));
}

static void* get_user_ptr(GcHeader* header) {
    if (!header) return NULL;
    return (char*)header + sizeof(GcHeader);
}

static bool is_pointer_valid(InfraxMemoryGc* self, void* ptr) {
    if (!self || !ptr) return false;
    
    // Check if pointer is within heap bounds
    char* heap_end = (char*)self->heap_start + self->heap_size;
    if ((char*)ptr < (char*)self->heap_start || (char*)ptr >= heap_end) {
        return false;
    }
    
    // Check alignment
    if ((uintptr_t)ptr % sizeof(void*) != 0) {
        return false;
    }
    
    // Check if pointer points to a valid object
    GcHeader* header = get_header(ptr);
    GcHeader* current = self->objects;
    
    while (current) {
        if (current == header) return true;
        current = current->next;
    }
    
    return false;
}
