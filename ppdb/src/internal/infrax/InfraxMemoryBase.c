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

// 基础内存管理器的私有数据
typedef struct {
    InfraxMemoryStats stats;
} BasePrivate;

// Basic memory operations
static void* base_alloc(InfraxMemoryBase* base, size_t size) {
    if (!base) return NULL;
    BasePrivate* priv = (BasePrivate*)base->private_data;
    void* ptr = malloc(size);
    if (ptr) {
        priv->stats.total_allocations++;
        priv->stats.current_usage += size;
        if (priv->stats.current_usage > priv->stats.peak_usage) {
            priv->stats.peak_usage = priv->stats.current_usage;
        }
    }
    return ptr;
}

static void* base_realloc(InfraxMemoryBase* base, void* ptr, size_t new_size) {
    if (!base) return NULL;
    BasePrivate* priv = (BasePrivate*)base->private_data;
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr) {
        priv->stats.total_allocations++;
        priv->stats.current_usage += new_size;
        if (priv->stats.current_usage > priv->stats.peak_usage) {
            priv->stats.peak_usage = priv->stats.current_usage;
        }
    }
    return new_ptr;
}

static void base_dealloc(InfraxMemoryBase* base, void* ptr) {
    if (!base || !ptr) return;
    BasePrivate* priv = (BasePrivate*)base->private_data;
    free(ptr);
    priv->stats.total_deallocations++;
}

static void* base_memset(InfraxMemoryBase* base, void* ptr, int value, size_t size) {
    if (!base || !ptr) return NULL;
    return memset(ptr, value, size);
}

static void base_get_stats(InfraxMemoryBase* base, InfraxMemoryStats* stats) {
    if (!base || !stats) return;
    BasePrivate* priv = (BasePrivate*)base->private_data;
    *stats = priv->stats;
}

static void base_reset_stats(InfraxMemoryBase* base) {
    if (!base) return;
    BasePrivate* priv = (BasePrivate*)base->private_data;
    memset(&priv->stats, 0, sizeof(InfraxMemoryStats));
}

// 构造函数
InfraxMemoryBase* infrax_memory_base_new(void) {
    InfraxMemoryBase* self = (InfraxMemoryBase*)malloc(sizeof(InfraxMemoryBase));
    if (!self) return NULL;
    
    infrax_memory_base_init(self);
    
    return self;
}

// 析构函数
void infrax_memory_base_free(InfraxMemoryBase* self) {
    if (!self) return;
    if (self->private_data) {
        free(self->private_data);
    }
    free(self);
}

// Initialize base memory interface
void infrax_memory_base_init(InfraxMemoryBase* base) {
    if (!base) return;
    
    // Allocate private data
    base->private_data = malloc(sizeof(BasePrivate));
    if (!base->private_data) return;
    memset(base->private_data, 0, sizeof(BasePrivate));
    
    // Initialize function pointers
    base->alloc = base_alloc;
    base->realloc = base_realloc;
    base->dealloc = base_dealloc;
    base->memset = base_memset;
    base->get_stats = base_get_stats;
    base->reset_stats = base_reset_stats;
}
