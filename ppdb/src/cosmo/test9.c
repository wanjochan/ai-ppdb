#include "cosmopolitan.h"

/* 内存管理 */
static char memory_pool[4096] __attribute__((section(".data"), aligned(4096)));
static size_t memory_used = 0;

/* 导出函数 */
__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int core_init(void) {
    memory_used = 0;
    memset(memory_pool, 0, sizeof(memory_pool));
    return 0;
}

__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
void* core_alloc(size_t size) {
    if (size == 0 || size > 4096) {
        return NULL;
    }
    
    /* 确保 8 字节对齐 */
    size = (size + 7) & ~7;
    
    if (memory_used + size > sizeof(memory_pool)) {
        return NULL;
    }
    
    void* ptr = &memory_pool[memory_used];
    memory_used += size;
    return ptr;
}

__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int net_connect(void) {
    return 42;  /* 返回一个固定值表示成功 */
}

__attribute__((section(".text"), visibility("default"), used, noinline, externally_visible))
int net_send(void* data) {
    if (!data) {
        return -1;
    }
    
    /* 检查指针是否在内存池范围内 */
    if ((char*)data < memory_pool || (char*)data >= memory_pool + sizeof(memory_pool)) {
        return -1;
    }
    
    /* 计算数据在内存池中的偏移 */
    size_t offset = (char*)data - memory_pool;
    return offset;
} 