#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "InfraxMemoryGc.h"

// 私有函数声明
static void* gc_alloc(InfraxMemoryBase* base, size_t size);
static void* gc_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size);
static void gc_dealloc(InfraxMemoryBase* base, void* ptr);
static void* gc_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size);
static void gc_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats);
static void gc_reset_stats(InfraxMemoryBase* base);

static bool should_trigger_gc(InfraxMemoryGc* self);
static void mark_phase(InfraxMemoryGc* self);
static void sweep_phase(InfraxMemoryGc* self);
static void scan_stack(InfraxMemoryGc* self);
static void scan_memory_region(InfraxMemoryGc* self, void* start, void* end);
static void mark_object(InfraxMemoryGc* self, void* ptr);
static GcHeader* get_header(void* ptr);
static void* get_user_ptr(GcHeader* header);
static bool is_pointer_valid(InfraxMemoryGc* self, void* ptr);

// 构造函数
InfraxMemoryGc* infrax_memory_gc_new(void) {
    InfraxMemoryGc* self = (InfraxMemoryGc*)malloc(sizeof(InfraxMemoryGc));
    if (!self) return NULL;
    
    // 初始化基础结构
    self->base.new = (InfraxMemoryBase* (*)(void))infrax_memory_gc_new;
    self->base.free = (void (*)(InfraxMemoryBase*))infrax_memory_gc_free;
    self->base.alloc = gc_alloc;
    self->base.realloc = gc_realloc;
    self->base.dealloc = gc_dealloc;
    self->base.memset = gc_memset;
    self->base.get_stats = gc_get_stats;
    self->base.reset_stats = gc_reset_stats;
    
    // 初始化GC配置
    self->config.initial_heap_size = 1024 * 1024;  // 1MB
    self->config.gc_threshold = 1024 * 512;        // 512KB
    self->config.enable_debug = false;
    
    // 初始化统计信息
    memset(&self->stats, 0, sizeof(InfraxMemoryGcStats));
    
    // 初始化GC状态
    self->heap_start = NULL;
    self->heap_size = 0;
    self->objects = NULL;
    self->stack_bottom = NULL;
    
    return self;
}

// 析构函数
void infrax_memory_gc_free(InfraxMemoryGc* self) {
    if (!self) return;
    
    // 释放所有对象
    GcHeader* current = self->objects;
    while (current) {
        GcHeader* next = current->next;
        free(current);
        current = next;
    }
    
    if (self->heap_start) {
        free(self->heap_start);
    }
    
    free(self);
}

// 内存分配
static void* gc_alloc(InfraxMemoryBase* base, size_t size) {
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    if (!self || size == 0) return NULL;
    
    // 检查是否需要GC
    if (should_trigger_gc(self)) {
        uint64_t start_time = (uint64_t)time(NULL) * 1000;
        self->collect(self);
        self->stats.last_gc_time_ms = (uint64_t)time(NULL) * 1000 - start_time;
    }
    
    // 分配内存
    GcHeader* header = (GcHeader*)malloc(sizeof(GcHeader) + size);
    if (!header) return NULL;
    
    header->size = size;
    header->marked = false;
    header->next = self->objects;
    self->objects = header;
    
    // 更新统计信息
    self->stats.base_stats.current_usage += size;
    self->stats.base_stats.total_allocations++;
    if (self->stats.base_stats.current_usage > self->stats.base_stats.peak_usage) {
        self->stats.base_stats.peak_usage = self->stats.base_stats.current_usage;
    }
    
    return get_user_ptr(header);
}

// 内存重分配
static void* gc_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size) {
    if (!base) return NULL;
    if (!ptr) return base->alloc(base, new_size);
    if (new_size == 0) {
        base->dealloc(base, ptr);
        return NULL;
    }
    
    GcHeader* header = get_header(ptr);
    if (new_size <= header->size) return ptr;
    
    void* new_ptr = base->alloc(base, new_size);
    if (!new_ptr) return NULL;
    
    memcpy(new_ptr, ptr, header->size);
    base->dealloc(base, ptr);
    
    return new_ptr;
}

// 内存释放（在GC模式下，这个函数实际上不会立即释放内存）
static void gc_dealloc(InfraxMemoryBase* base, void* ptr) {
    // 在GC模式下，我们不直接释放内存，而是等待GC来处理
    if (!base || !ptr) return;
    
    // 但我们可以更新一些统计信息
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    GcHeader* header = get_header(ptr);
    self->stats.base_stats.current_usage -= header->size;
}

// 内存设置
static void* gc_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size) {
    if (!base || !ptr) return NULL;
    return memset(ptr, value, size);
}

// 检查是否应该触发GC
static bool should_trigger_gc(InfraxMemoryGc* self) {
    if (!self) return false;
    
    // 当前内存使用量超过阈值
    return self->stats.base_stats.current_usage >= self->config.gc_threshold;
}

// 标记阶段
static void mark_phase(InfraxMemoryGc* self) {
    if (!self) return;
    
    // 从根集开始标记
    scan_stack(self);
    
    // TODO: 标记全局变量和其他根集
}

// 扫描栈空间
static void scan_stack(InfraxMemoryGc* self) {
    if (!self || !self->stack_bottom) return;
    
    void* stack_top = &stack_top;  // 获取当前栈顶
    scan_memory_region(self, stack_top, self->stack_bottom);
}

// 扫描内存区域
static void scan_memory_region(InfraxMemoryGc* self, void* start, void* end) {
    if (!self || !start || !end) return;
    
    void** current = (void**)start;
    while (current < (void**)end) {
        void* ptr = *current;
        if (is_pointer_valid(self, ptr)) {
            mark_object(self, ptr);
        }
        current++;
    }
}

// 标记对象
static void mark_object(InfraxMemoryGc* self, void* ptr) {
    if (!self || !ptr) return;
    
    GcHeader* header = get_header(ptr);
    if (!header || header->marked) return;
    
    header->marked = true;
    
    // 递归标记对象中的指针
    scan_memory_region(self, ptr, (char*)ptr + header->size);
}

// 清除阶段
static void sweep_phase(InfraxMemoryGc* self) {
    if (!self) return;
    
    GcHeader** current = &self->objects;
    while (*current) {
        GcHeader* header = *current;
        
        if (!header->marked) {
            // 未标记的对象需要被清除
            *current = header->next;
            
            // 更新统计信息
            self->stats.total_freed += header->size;
            self->stats.total_collections++;
            
            free(header);
        } else {
            // 重置标记，为下次GC做准备
            header->marked = false;
            current = &header->next;
        }
    }
}

// 执行GC
static void gc_collect(InfraxMemoryGc* self) {
    if (!self) return;
    
    mark_phase(self);
    sweep_phase(self);
}

// 获取对象头
static GcHeader* get_header(void* ptr) {
    if (!ptr) return NULL;
    return (GcHeader*)((char*)ptr - sizeof(GcHeader));
}

// 获取用户指针
static void* get_user_ptr(GcHeader* header) {
    if (!header) return NULL;
    return (char*)header + sizeof(GcHeader);
}

// 检查指针是否有效
static bool is_pointer_valid(InfraxMemoryGc* self, void* ptr) {
    if (!self || !ptr) return false;
    
    // 检查指针是否在堆范围内
    if (ptr < self->heap_start || ptr >= (char*)self->heap_start + self->heap_size) {
        return false;
    }
    
    // 检查是否对齐
    if ((uintptr_t)ptr % sizeof(void*) != 0) {
        return false;
    }
    
    // 检查是否指向对象头
    GcHeader* header = get_header(ptr);
    GcHeader* current = self->objects;
    while (current) {
        if (current == header) return true;
        current = current->next;
    }
    
    return false;
}

// 获取统计信息
static void gc_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats) {
    if (!base || !stats) return;
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    memcpy(stats, &self->stats.base_stats, sizeof(InfraxMemoryStats));
}

// 重置统计信息
static void gc_reset_stats(InfraxMemoryBase* base) {
    if (!base) return;
    InfraxMemoryGc* self = (InfraxMemoryGc*)base;
    memset(&self->stats, 0, sizeof(InfraxMemoryGcStats));
}
