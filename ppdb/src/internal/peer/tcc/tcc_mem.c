#include "tcc_mem.h"
#include "internal/infra/infra_memory.h"

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static void* align_ptr(void* ptr, size_t align) {
    return (void*)(((uintptr_t)ptr + align - 1) & ~(align - 1));
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

tcc_mem_block_t* tcc_mem_alloc(size_t size, unsigned int flags) {
    // 分配内存块结构
    tcc_mem_block_t* block = infra_malloc(sizeof(tcc_mem_block_t));
    if (!block) {
        return NULL;
    }

    // 分配实际内存
    void* ptr = infra_malloc(size);
    if (!ptr) {
        infra_free(block);
        return NULL;
    }

    // 初始化内存块
    block->ptr = ptr;
    block->size = size;
    block->flags = flags;

    // 设置内存属性
    if (tcc_mem_protect(block, flags) < 0) {
        infra_free(ptr);
        infra_free(block);
        return NULL;
    }

    return block;
}

void tcc_mem_free(tcc_mem_block_t* block) {
    if (!block) {
        return;
    }

    if (block->ptr) {
        infra_free(block->ptr);
    }
    infra_free(block);
}

int tcc_mem_protect(tcc_mem_block_t* block, unsigned int flags) {
    if (!block || !block->ptr) {
        return -1;
    }

    // 转换标志位
    unsigned int infra_flags = 0;
    if (flags & TCC_MEM_READ) {
        infra_flags |= INFRA_MEM_READ;
    }
    if (flags & TCC_MEM_WRITE) {
        infra_flags |= INFRA_MEM_WRITE;
    }
    if (flags & TCC_MEM_EXEC) {
        infra_flags |= INFRA_MEM_EXEC;
    }

    // 设置内存保护
    infra_error_t err = infra_mem_protect(block->ptr, block->size, infra_flags);
    if (err != INFRA_OK) {
        return -1;
    }

    block->flags = flags;
    return 0;
}

int tcc_mem_copy(tcc_mem_block_t* dst, const tcc_mem_block_t* src) {
    if (!dst || !src || !dst->ptr || !src->ptr) {
        return -1;
    }

    if (dst->size < src->size) {
        return -1;
    }

    // 复制内存内容
    memcpy(dst->ptr, src->ptr, src->size);
    return 0;
}

// Basic memory allocation functions using infra layer
void *tcc_malloc(size_t size)
{
    return infra_malloc(size);
}

void *tcc_mallocz(size_t size)
{
    void *ptr = infra_malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void *tcc_realloc(void *ptr, size_t size)
{
    return infra_realloc(ptr, size);
}

void tcc_free(void *ptr)
{
    infra_free(ptr);
}

// Memory pool functions
struct mem_pool {
    size_t size;
    uint8_t *cur_ptr;
    uint8_t *end_ptr;
    struct mem_pool *next;
};

mem_pool_t *tcc_pool_new(size_t size)
{
    mem_pool_t *pool = tcc_malloc(sizeof(mem_pool_t) + size);
    if (!pool) {
        return NULL;
    }
    
    pool->size = size;
    pool->cur_ptr = (uint8_t *)(pool + 1);
    pool->end_ptr = pool->cur_ptr + size;
    pool->next = NULL;
    
    return pool;
}

void tcc_pool_delete(mem_pool_t *pool)
{
    while (pool) {
        mem_pool_t *next = pool->next;
        tcc_free(pool);
        pool = next;
    }
}

void *tcc_pool_malloc(mem_pool_t *pool, size_t size)
{
    // Align size to pointer boundary
    size = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
    
    if (pool->cur_ptr + size > pool->end_ptr) {
        return NULL;
    }
    
    void *ptr = pool->cur_ptr;
    pool->cur_ptr += size;
    return ptr;
}