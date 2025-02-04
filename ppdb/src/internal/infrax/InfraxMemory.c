#include <stdlib.h>
#include <string.h>
#include "InfraxMemory.h"

// 私有函数声明
static void* memory_alloc(InfraxMemory* self, size_t size);
static void* memory_realloc(InfraxMemory* self, void* ptr, size_t new_size);
static void memory_dealloc(InfraxMemory* self, void* ptr);
static void* memory_memset(InfraxMemory* self, void* ptr, int value, size_t size);
static void memory_get_stats(InfraxMemory* self, InfraxMemoryStats* stats);
static void memory_set_config(InfraxMemory* self, const InfraxMemoryConfig* config);

// 构造函数
InfraxMemory* infrax_memory_new(void) {
    InfraxMemory* self = (InfraxMemory*)malloc(sizeof(InfraxMemory));
    if (!self) return NULL;
    
    // 初始化方法
    self->new = infrax_memory_new;
    self->free = infrax_memory_free;
    self->alloc = memory_alloc;
    self->realloc = memory_realloc;
    self->dealloc = memory_dealloc;
    self->memset = memory_memset;
    self->get_stats = memory_get_stats;
    self->set_config = memory_set_config;
    
    // 默认使用基础内存管理
    self->mode = MEMORY_MODE_BASE;
    self->base = infrax_memory_base_new();
    
    return self;
}

// 析构函数
void infrax_memory_free(InfraxMemory* self) {
    if (!self) return;
    
    // 释放当前实现
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            if (self->base) self->base->free(self->base);
            break;
        case MEMORY_MODE_POOL:
            if (self->pool) self->pool->base.free((InfraxMemoryBase*)self->pool);
            break;
        case MEMORY_MODE_GC:
            if (self->gc) self->gc->base.free((InfraxMemoryBase*)self->gc);
            break;
    }
    free(self);
}

// 设置配置
static void memory_set_config(InfraxMemory* self, const InfraxMemoryConfig* config) {
    if (!self || !config) return;
    
    // 如果模式改变，需要清理旧的实现并创建新的实现
    if (self->mode != config->mode) {
        // 清理旧的实现
        switch (self->mode) {
            case MEMORY_MODE_BASE:
                if (self->base) self->base->free(self->base);
                break;
            case MEMORY_MODE_POOL:
                if (self->pool) self->pool->base.free((InfraxMemoryBase*)self->pool);
                break;
            case MEMORY_MODE_GC:
                if (self->gc) self->gc->base.free((InfraxMemoryBase*)self->gc);
                break;
        }
        
        // 创建新的实现
        switch (config->mode) {
            case MEMORY_MODE_BASE:
                self->base = infrax_memory_base_new();
                break;
            case MEMORY_MODE_POOL:
                self->pool = infrax_memory_pool_new();
                if (self->pool) {
                    self->pool->set_config(self->pool, &config->pool_config);
                }
                break;
            case MEMORY_MODE_GC:
                self->gc = infrax_memory_gc_new();
                if (self->gc) {
                    self->gc->set_config(self->gc, &config->gc_config);
                }
                break;
        }
        self->mode = config->mode;
    }
}

// 内存分配
static void* memory_alloc(InfraxMemory* self, size_t size) {
    if (!self) return NULL;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            return self->base ? self->base->alloc(self->base, size) : NULL;
        case MEMORY_MODE_POOL:
            return self->pool ? self->pool->base.alloc((InfraxMemoryBase*)self->pool, size) : NULL;
        case MEMORY_MODE_GC:
            return self->gc ? self->gc->base.alloc((InfraxMemoryBase*)self->gc, size) : NULL;
        default:
            return NULL;
    }
}

// 内存重分配
static void* memory_realloc(InfraxMemory* self, void* ptr, size_t new_size) {
    if (!self) return NULL;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            return self->base ? self->base->realloc(self->base, ptr, new_size) : NULL;
        case MEMORY_MODE_POOL:
            return self->pool ? self->pool->base.realloc((InfraxMemoryBase*)self->pool, ptr, new_size) : NULL;
        case MEMORY_MODE_GC:
            return self->gc ? self->gc->base.realloc((InfraxMemoryBase*)self->gc, ptr, new_size) : NULL;
        default:
            return NULL;
    }
}

// 内存释放
static void memory_dealloc(InfraxMemory* self, void* ptr) {
    if (!self || !ptr) return;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            if (self->base) self->base->dealloc(self->base, ptr);
            break;
        case MEMORY_MODE_POOL:
            if (self->pool) self->pool->base.dealloc((InfraxMemoryBase*)self->pool, ptr);
            break;
        case MEMORY_MODE_GC:
            if (self->gc) self->gc->base.dealloc((InfraxMemoryBase*)self->gc, ptr);
            break;
    }
}

// 内存设置
static void* memory_memset(InfraxMemory* self, void* ptr, int value, size_t size) {
    if (!self || !ptr) return NULL;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            return self->base ? self->base->memset(self->base, ptr, value, size) : NULL;
        case MEMORY_MODE_POOL:
            return self->pool ? self->pool->base.memset((InfraxMemoryBase*)self->pool, ptr, value, size) : NULL;
        case MEMORY_MODE_GC:
            return self->gc ? self->gc->base.memset((InfraxMemoryBase*)self->gc, ptr, value, size) : NULL;
        default:
            return NULL;
    }
}

// 获取统计信息
static void memory_get_stats(InfraxMemory* self, InfraxMemoryStats* stats) {
    if (!self || !stats) return;
    
    switch (self->mode) {
        case MEMORY_MODE_BASE:
            if (self->base) self->base->get_stats(self->base, stats);
            break;
        case MEMORY_MODE_POOL:
            if (self->pool) self->pool->base.get_stats((InfraxMemoryBase*)self->pool, stats);
            break;
        case MEMORY_MODE_GC:
            if (self->gc) self->gc->base.get_stats((InfraxMemoryBase*)self->gc, stats);
            break;
    }
}
