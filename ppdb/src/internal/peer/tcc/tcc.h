#ifndef TCC_H
#define TCC_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define TCC_OUTPUT_MEMORY    1  // 输出到内存
#define TCC_OUTPUT_EXE       2  // 输出到可执行文件
#define TCC_RELOCATE_AUTO    1  // 自动重定位

#define TCC_MAX_SYMBOL_NAME  256
#define TCC_MAX_PATH        1024

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 符号表项
typedef struct {
    char name[TCC_MAX_SYMBOL_NAME];
    void* addr;
} tcc_symbol_t;

// TCC 状态
typedef struct {
    // 输出类型
    int output_type;
    
    // 符号表
    tcc_symbol_t* symbols;
    int symbol_count;
    int symbol_capacity;
    
    // 代码段
    void* code;
    size_t code_size;
    
    // 错误处理
    void (*error_func)(void* opaque, const char* msg);
    void* error_opaque;
    
    // 内存管理
    void* mem_ptr;
    size_t mem_size;
} tcc_state_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 创建 TCC 状态
tcc_state_t* tcc_new(void);

// 删除 TCC 状态
void tcc_delete(tcc_state_t* s);

// 设置错误处理函数
void tcc_set_error_func(tcc_state_t* s, void* opaque, void (*error_func)(void* opaque, const char* msg));

// 设置输出类型
void tcc_set_output_type(tcc_state_t* s, int output_type);

// 添加源文件
int tcc_add_file(tcc_state_t* s, const char* filename);

// 重定位代码
int tcc_relocate(tcc_state_t* s, void* ptr);

// 获取符号地址
void* tcc_get_symbol(tcc_state_t* s, const char* name);

// 添加符号
int tcc_add_symbol(tcc_state_t* s, const char* name, const void* val);

#endif // TCC_H