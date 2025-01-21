#ifndef POLY_TCC_H
#define POLY_TCC_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"

// 内存保护标志
#define POLY_TCC_PROT_NONE  INFRA_PROT_NONE
#define POLY_TCC_PROT_READ  INFRA_PROT_READ
#define POLY_TCC_PROT_WRITE INFRA_PROT_WRITE
#define POLY_TCC_PROT_EXEC  INFRA_PROT_EXEC

// TCC 状态结构
typedef struct poly_tcc_state {
    // 符号表
    char **symbol_names;      // 符号名称数组
    void **symbol_addrs;      // 符号地址数组
    size_t symbol_count;      // 符号数量
    size_t symbol_capacity;   // 符号容量

    // 代码段
    void *code;              // 代码段地址
    size_t code_size;        // 代码段大小
    size_t code_capacity;    // 代码段容量

    // 数据段
    void *data;              // 数据段地址
    size_t data_size;        // 数据段大小
    size_t data_capacity;    // 数据段容量

    // 错误处理
    char error_msg[256];     // 错误消息
} poly_tcc_state_t;

// TCC 状态管理
poly_tcc_state_t* poly_tcc_new(void);
void poly_tcc_delete(poly_tcc_state_t *s);

// 编译和执行
int poly_tcc_compile_string(poly_tcc_state_t *s, const char *str);
int poly_tcc_run(poly_tcc_state_t *s, int argc, char **argv);

// 符号管理
int poly_tcc_add_symbol(poly_tcc_state_t *s, const char *name, const void *val);
void* poly_tcc_get_symbol(poly_tcc_state_t *s, const char *name);

// 错误处理
const char* poly_tcc_get_error_msg(poly_tcc_state_t *s);

// 内存管理
void* poly_tcc_malloc(size_t size);
void poly_tcc_free(void *ptr);

// 内存管理函数
void* poly_tcc_mmap(void *addr, size_t size, int prot);
infra_error_t poly_tcc_munmap(void *ptr, size_t size);
infra_error_t poly_tcc_mprotect(void *ptr, size_t size, int prot);

#endif // POLY_TCC_H 