#ifndef TCC_H
#define TCC_H

#include "internal/infra/infra_core.h"

// TCC 状态结构
typedef struct tcc_state {
    // 符号表
    infra_symbol_t *symbols;
    size_t symbol_count;

    // 代码段
    void *code;
    size_t code_size;
    size_t code_capacity;

    // 数据段
    void *data;
    size_t data_size;
    size_t data_capacity;

    // 错误处理
    char error_msg[256];
} tcc_state_t;

// TCC 状态管理
tcc_state_t* tcc_new(void);
void tcc_delete(tcc_state_t *s);

// 编译和执行
int tcc_compile_string(tcc_state_t *s, const char *str);
int tcc_run(tcc_state_t *s, int argc, char **argv);

// 符号管理
int tcc_add_symbol(tcc_state_t *s, const char *name, const void *val);
void* tcc_get_symbol(tcc_state_t *s, const char *name);

// 错误处理
const char* tcc_get_error_msg(tcc_state_t *s);

#endif // TCC_H 