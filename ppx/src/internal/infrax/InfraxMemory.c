#include "InfraxMemory.h"

// Forward declarations of instance methods
static void* infrax_memory_alloc(InfraxMemory* self, size_t size);
static void infrax_memory_dealloc(InfraxMemory* self, void* ptr);
static void* infrax_memory_realloc(InfraxMemory* self, void* ptr, size_t size);
static void infrax_memory_get_stats(const InfraxMemory* self, InfraxMemoryStats* stats);
static void infrax_memory_collect(InfraxMemory* self);

// Constructor implementation
static InfraxMemory* infrax_memory_new(const InfraxMemoryConfig* config) {
    if (!config) return NULL;

    // Allocate memory manager
    InfraxMemory* self = gInfraxCore.malloc(&gInfraxCore, sizeof(InfraxMemory));
    if (!self) return NULL;

    self->self = self;
    // self->klass = &InfraxMemoryClass;

    // Initialize instance methods
    self->alloc = infrax_memory_alloc;
    self->dealloc = infrax_memory_dealloc;
    self->realloc = infrax_memory_realloc;
    self->get_stats = infrax_memory_get_stats;
    self->collect = infrax_memory_collect;

    // Copy configuration
    self->config = *config;

    // Initialize memory pool
    if (self->config.use_pool) {
        self->pool_start = gInfraxCore.malloc(&gInfraxCore, self->config.initial_size);
        if (self->pool_start) {
            self->pool_size = self->config.initial_size;
            self->free_list = self->pool_start;
            self->free_list->size = self->config.initial_size - sizeof(MemoryBlock);
            self->free_list->is_used = INFRAX_FALSE;
            self->free_list->is_gc_root = INFRAX_FALSE;
            self->free_list->next = NULL;
        }
    }

    // Initialize stats
    gInfraxCore.memset(&gInfraxCore,&self->stats, 0, sizeof(InfraxMemoryStats));

    return self;
}

// Destructor implementation
static void infrax_memory_free(InfraxMemory* self) {
    if (!self) return;
    
    // Free memory pool
    if (self->pool_start) {
        gInfraxCore.free(&gInfraxCore, self->pool_start);
    }
    
    // Free GC objects
    MemoryBlock* obj = self->gc_objects;
    while (obj) {
        MemoryBlock* next = obj->next;
        gInfraxCore.free(&gInfraxCore, obj);
        obj = next;
    }
    
    gInfraxCore.free(&gInfraxCore, self);
}

// The "static" interface implementation
InfraxMemoryClassType InfraxMemoryClass = {
    .new = infrax_memory_new,
    .free = infrax_memory_free
};

// Helper functions for memory pool
static MemoryBlock* find_best_fit(InfraxMemory* self, size_t size) {
    MemoryBlock* best = NULL;
    MemoryBlock* current = self->free_list;
    size_t min_size = (size_t)-1;

    while (current) {
        if (!current->is_used && current->size >= size) {
            if (current->size < min_size) {
                min_size = current->size;
                best = current;
            }
        }
        current = current->next;
    }
    return best;
}

static void split_block(MemoryBlock* block, size_t size) {
    size_t total_size = block->size;
    size_t remaining = total_size - size - sizeof(MemoryBlock);
    
    if (remaining >= sizeof(MemoryBlock) + 8) {
        MemoryBlock* new_block = (MemoryBlock*)((char*)block + sizeof(MemoryBlock) + size);
        new_block->size = remaining - sizeof(MemoryBlock);
        new_block->is_used = INFRAX_FALSE;
        new_block->is_gc_root = INFRAX_FALSE;
        new_block->next = block->next;
        
        block->size = size;
        block->next = new_block;
    }
}

static void merge_free_blocks(InfraxMemory* self) {
    MemoryBlock* current = self->free_list;
    
    while (current && current->next) {
        if (!current->is_used && !current->next->is_used) {
            current->size += sizeof(MemoryBlock) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

// Helper functions for GC
static void mark_from_roots(InfraxMemory* self) {
    MemoryBlock* obj = self->gc_objects;
    while (obj) {
        if (obj->is_gc_root) {
            obj->is_used = INFRAX_TRUE;
        }
        obj = obj->next;
    }
}

static void sweep_unused(InfraxMemory* self) {
    MemoryBlock** obj_ptr = &self->gc_objects;
    while (*obj_ptr) {
        MemoryBlock* obj = *obj_ptr;
        if (!obj->is_used && !obj->is_gc_root) {
            *obj_ptr = obj->next;
            self->stats.total_deallocations++;
            self->stats.current_usage -= obj->size;
            gInfraxCore.free(&gInfraxCore, obj);
        } else {
            obj->is_used = INFRAX_FALSE;  // Reset for next collection
            obj_ptr = &obj->next;
        }
    }
}

// Core functions implementation
static void* infrax_memory_alloc(InfraxMemory* self, size_t size) {
    if (!self || size == 0) return NULL;
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    void* ptr = NULL;
    if (self->config.use_pool) {
        // Try pool allocation first
        MemoryBlock* block = find_best_fit(self, size);
        if (block) {
            split_block(block, size);
            block->is_used = INFRAX_TRUE;
            block->is_gc_root = self->config.use_gc;
            ptr = (char*)block + sizeof(MemoryBlock);
        }
    }
    
    if (!ptr) {
        // Fallback to direct allocation
        size_t total_size = sizeof(MemoryBlock) + size;
        MemoryBlock* block = gInfraxCore.malloc(&gInfraxCore, total_size);
        if (!block) return NULL;
        
        block->size = size;
        block->is_used = INFRAX_TRUE;
        block->is_gc_root = self->config.use_gc;
        
        if (self->config.use_gc) {
            block->next = self->gc_objects;
            self->gc_objects = block;
        } else {
            block->next = NULL;
        }
        
        ptr = (char*)block + sizeof(MemoryBlock);
    }
    
    if (ptr) {
        self->stats.total_allocations++;
        self->stats.current_usage += size;
        if (self->stats.current_usage > self->stats.peak_usage) {
            self->stats.peak_usage = self->stats.current_usage;
        }
    }
    
    return ptr;
}

static void infrax_memory_dealloc(InfraxMemory* self, void* ptr) {
    if (!self || !ptr) return;
    
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    size_t block_size = block->size;  // 保存大小以便后续使用
    
    if (self->config.use_pool) {
        // Check if ptr is in pool range
        if ((char*)ptr >= (char*)self->pool_start && 
            (char*)ptr < (char*)self->pool_start + self->pool_size) {
            block->is_used = INFRAX_FALSE;
            merge_free_blocks(self);
            self->stats.total_deallocations++;
            // 检查减法是否会导致溢出
            if (self->stats.current_usage >= block_size) {
                self->stats.current_usage -= block_size;
            } else {
                self->stats.current_usage = 0;
            }
            return;
        }
    }
    
    if (!self->config.use_gc) {
        self->stats.total_deallocations++;
        // 检查减法是否会导致溢出
        if (self->stats.current_usage >= block_size) {
            self->stats.current_usage -= block_size;
        } else {
            self->stats.current_usage = 0;
        }
        gInfraxCore.free(&gInfraxCore, block);
    }
    // If using GC, memory will be freed during collection
}

static void* infrax_memory_realloc(InfraxMemory* self, void* ptr, size_t size) {
    if (!ptr) return infrax_memory_alloc(self, size);
    if (size == 0) {
        infrax_memory_dealloc(self, ptr);
        return NULL;
    }
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    MemoryBlock* block = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    
    // 如果新的大小小于或等于当前大小,直接返回
    if (size <= block->size) {
        return ptr;
    }
    
    // 检查是否在内存池中
    InfraxBool is_pool_block = (self->config.use_pool && 
                         (char*)ptr >= (char*)self->pool_start && 
                         (char*)ptr < (char*)self->pool_start + self->pool_size) ? INFRAX_TRUE : INFRAX_FALSE;
    
    if (is_pool_block == INFRAX_TRUE) {
        // 尝试合并后面的空闲块
        size_t needed_size = size - block->size;
        MemoryBlock* next = block->next;
        
        if (next && next->is_used == INFRAX_FALSE && 
            (sizeof(MemoryBlock) + next->size) >= needed_size) {
            // 可以直接扩展当前块
            size_t remaining = next->size - needed_size;
            if (remaining >= sizeof(MemoryBlock) + 8) {
                // 分割剩余空间
                MemoryBlock* new_next = (MemoryBlock*)((char*)next + needed_size);
                new_next->size = remaining - sizeof(MemoryBlock);
                new_next->is_used = INFRAX_FALSE;
                new_next->is_gc_root = INFRAX_FALSE;
                new_next->next = next->next;
                block->next = new_next;
            } else {
                // 全部使用
                block->next = next->next;
            }
            block->size = size;
            return ptr;
        }
    }
    
    // 分配新内存并复制数据
    void* new_ptr = infrax_memory_alloc(self, size);
    if (!new_ptr) return NULL;
    
    gInfraxCore.memcpy(&gInfraxCore, new_ptr, ptr, block->size);
    infrax_memory_dealloc(self, ptr);
    return new_ptr;
}

static void infrax_memory_get_stats(const InfraxMemory* self, InfraxMemoryStats* stats) {
    if (!self || !stats) return;
    gInfraxCore.memcpy(&gInfraxCore, stats, &self->stats, sizeof(InfraxMemoryStats));
}

static void infrax_memory_collect(InfraxMemory* self) {
    if (!self || !self->config.use_gc) return;
    
    if (self->stats.current_usage < self->config.gc_threshold) {
        return;  // No need to collect yet
    }
    
    mark_from_roots(self);
    sweep_unused(self);
}
