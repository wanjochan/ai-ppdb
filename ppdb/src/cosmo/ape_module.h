#ifndef _APE_MODULE_H_
#define _APE_MODULE_H_

#include "cosmopolitan.h"

typedef struct {
    void* base;           // 模块基地址
    size_t size;         // 模块大小
    void* entry;         // 入口点
    Elf64_Sym* symtab;   // 符号表
    char* strtab;        // 字符串表
    Elf64_Rela* rela;    // 重定位表
    size_t rela_count;   // 重定位条目数
    size_t sym_count;    // 符号数量
} ape_module_t;

// 加载APE模块
ape_module_t* load_ape_module(const char* path);

// 查找符号
void* find_symbol(ape_module_t* mod, const char* name);

// 卸载模块
void unload_ape_module(ape_module_t* mod);

#endif /* _APE_MODULE_H_ */