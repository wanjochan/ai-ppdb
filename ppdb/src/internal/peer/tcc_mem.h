#ifndef TCC_MEM_H
#define TCC_MEM_H

#include "internal/infra/infra_memory.h"

// 内存块结构
typedef struct tcc_mem_block {
    void *ptr;          // 内存块指针
    size_t size;        // 内存块大小
    int prot;          // 内存保护标志
} tcc_mem_block_t;

// 内存管理函数
void* tcc_mem_alloc(size_t size);
void tcc_mem_free(void *ptr);

// 内存映射函数
void* tcc_mem_map(size_t size, int prot);
int tcc_mem_unmap(void *ptr, size_t size);

// 内存保护函数
int tcc_mem_protect(void *ptr, size_t size, int prot);

#endif // TCC_MEM_H 