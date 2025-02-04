#ifndef INFRAX_MEMORY_GC_H
#define INFRAX_MEMORY_GC_H

#include "InfraxMemoryBase.h"

// GC配置
typedef struct {
    size_t initial_heap_size;  // 初始堆大小
    size_t gc_threshold;       // GC触发阈值
    bool enable_debug;         // 是否启用调试
} InfraxMemoryGcConfig;

// GC统计信息
typedef struct {
    InfraxMemoryStats base_stats;  // 基础统计信息
    size_t total_freed;            // 总共释放的内存
    size_t total_collections;      // GC执行次数
    uint64_t last_gc_time_ms;     // 最后一次GC耗时
} InfraxMemoryGcStats;

// GC对象头
typedef struct GcHeader {
    size_t size;              // 对象大小
    bool marked;              // 标记位
    struct GcHeader* next;    // 下一个对象
} GcHeader;

// GC管理接口
typedef struct InfraxMemoryGc {
    // Base memory interface
    InfraxMemoryBase base;
    
    // GC specific members
    InfraxMemoryGcConfig config;
    InfraxMemoryGcStats stats;
    void* heap_start;         // 堆起始地址
    size_t heap_size;        // 堆大小
    GcHeader* objects;       // 对象链表
    void* stack_bottom;      // 栈底指针

    // GC specific methods
    void (*set_config)(struct InfraxMemoryGc*, const InfraxMemoryGcConfig* config);
    void (*get_gc_stats)(struct InfraxMemoryGc*, InfraxMemoryGcStats* stats);
    void (*collect)(struct InfraxMemoryGc*);  // 执行GC
    void (*init_with_stack)(struct InfraxMemoryGc*, void* stack_bottom);  // 初始化栈信息
} InfraxMemoryGc;

// Constructor and destructor
InfraxMemoryGc* infrax_memory_gc_new(void);
void infrax_memory_gc_free(InfraxMemoryGc* self);

#endif // INFRAX_MEMORY_GC_H
