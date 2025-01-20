#include "internal/peer/tcc_mem.h"

// 内存管理函数
void* tcc_mem_alloc(size_t size)
{
    return infra_mem_alloc(size);
}

void tcc_mem_free(void *ptr)
{
    infra_mem_free(ptr);
}

// 内存映射函数
void* tcc_mem_map(size_t size, int prot)
{
    return infra_mem_map(NULL, size, prot);
}

int tcc_mem_unmap(void *ptr, size_t size)
{
    return infra_mem_unmap(ptr, size);
}

// 内存保护函数
int tcc_mem_protect(void *ptr, size_t size, int prot)
{
    return infra_mem_protect(ptr, size, prot);
} 