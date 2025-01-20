#ifndef TCC_JIT_H
#define TCC_JIT_H

#include "tcc.h"
#include "infra/mem.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// JIT 编译选项
typedef struct {
    int optimize_level;     // 优化级别 (0-3)
    bool enable_debug;      // 是否启用调试信息
} tcc_jit_options_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 编译源代码
int tcc_jit_compile(tcc_state_t* s, const char* source_code);

// 编译源文件
int tcc_jit_compile_file(tcc_state_t* s, const char* filename);

// 设置编译选项
void tcc_jit_set_options(tcc_state_t* s, const tcc_jit_options_t* options);

// 获取编译错误信息
const char* tcc_jit_get_error(tcc_state_t* s);

/* JIT compilation context */
typedef struct TCCJitContext {
    TCCState *s;           /* TCC state */
    void *code_ptr;        /* Generated code pointer */
    size_t code_size;      /* Size of generated code */
    int (*entry_point)();  /* Entry point function pointer */
} TCCJitContext;

/* JIT compilation functions */
TCCJitContext* tcc_jit_new(void);
void tcc_jit_delete(TCCJitContext *ctx);

/* Core JIT operations */
int tcc_jit_run(TCCJitContext *ctx);

/* Memory management */
void* tcc_jit_get_code_ptr(TCCJitContext *ctx);
size_t tcc_jit_get_code_size(TCCJitContext *ctx);

#endif /* TCC_JIT_H */