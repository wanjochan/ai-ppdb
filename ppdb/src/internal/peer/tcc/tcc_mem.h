#ifndef TCC_MEM_H
#define TCC_MEM_H

#include "tcc.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 内存块信息
typedef struct {
    void* ptr;              // 内存块指针
    size_t size;           // 内存块大小
    unsigned int flags;    // 内存属性标志
} tcc_mem_block_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 分配内存块
tcc_mem_block_t* tcc_mem_alloc(size_t size, unsigned int flags);

// 释放内存块
void tcc_mem_free(tcc_mem_block_t* block);

// 设置内存块属性
int tcc_mem_protect(tcc_mem_block_t* block, unsigned int flags);

// 复制内存块
int tcc_mem_copy(tcc_mem_block_t* dst, const tcc_mem_block_t* src);

#endif // TCC_MEM_H