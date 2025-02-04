#include <stdlib.h>
#include <string.h>
#include "InfraxMemoryBase.h"

// 私有函数声明
static void* base_alloc(InfraxMemoryBase* self, size_t size);
static void* base_realloc(InfraxMemoryBase* self, void* ptr, size_t new_size);
static void base_dealloc(InfraxMemoryBase* self, void* ptr);
static void* base_memset(InfraxMemoryBase* self, void* ptr, int value, size_t size);
static void base_get_stats(InfraxMemoryBase* self, InfraxMemoryStats* stats);
static void base_reset_stats(InfraxMemoryBase* self);

// 构造函数
InfraxMemoryBase* infrax_memory_base_new(void) {
    InfraxMemoryBase* self = (InfraxMemoryBase*)malloc(sizeof(InfraxMemoryBase));
    if (!self) return NULL;
    
    // 初始化统计信息
    memset(&self->stats, 0, sizeof(InfraxMemoryStats));
    
    // 初始化方法
    self->new = infrax_memory_base_new;
    self->free = infrax_memory_base_free;
    self->alloc = base_alloc;
    self->realloc = base_realloc;
    self->dealloc = base_dealloc;
    self->memset = base_memset;
    self->get_stats = base_get_stats;
    self->reset_stats = base_reset_stats;
    
    return self;
}

// 析构函数
void infrax_memory_base_free(InfraxMemoryBase* self) {
    if (!self) return;
    free(self);
}

// 内存分配
static void* base_alloc(InfraxMemoryBase* self, size_t size) {
    if (!self || size == 0) return NULL;
    
    void* ptr = malloc(size);
    if (ptr) {
        self->stats.current_usage += size;
        self->stats.total_allocations++;
        if (self->stats.current_usage > self->stats.peak_usage) {
            self->stats.peak_usage = self->stats.current_usage;
        }
    }
    return ptr;
}

// 内存重分配
static void* base_realloc(InfraxMemoryBase* self, void* ptr, size_t new_size) {
    if (!self) return NULL;
    if (!ptr) return self->alloc(self, new_size);
    if (new_size == 0) {
        self->dealloc(self, ptr);
        return NULL;
    }
    
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr) {
        // 更新统计信息（这里简化处理，没有考虑原始大小）
        self->stats.current_usage += new_size;
        if (self->stats.current_usage > self->stats.peak_usage) {
            self->stats.peak_usage = self->stats.current_usage;
        }
    }
    return new_ptr;
}

// 内存释放
static void base_dealloc(InfraxMemoryBase* self, void* ptr) {
    if (!self || !ptr) return;
    free(ptr);
    // 注意：这里我们无法准确知道释放了多少内存，因为没有记录分配的大小
    // 在实际应用中，可能需要额外的数据结构来跟踪每个分配的大小
}

// 内存设置
static void* base_memset(InfraxMemoryBase* self, void* ptr, int value, size_t size) {
    if (!self || !ptr) return NULL;
    return memset(ptr, value, size);
}

// 获取统计信息
static void base_get_stats(InfraxMemoryBase* self, InfraxMemoryStats* stats) {
    if (!self || !stats) return;
    memcpy(stats, &self->stats, sizeof(InfraxMemoryStats));
}

// 重置统计信息
static void base_reset_stats(InfraxMemoryBase* self) {
    if (!self) return;
    memset(&self->stats, 0, sizeof(InfraxMemoryStats));
}
