#include "internal/peer/tcc.h"
#include "internal/peer/tcc_mem.h"

// 创建新的 TCC 状态
tcc_state_t* tcc_new(void)
{
    tcc_state_t *s = tcc_mem_alloc(sizeof(tcc_state_t));
    if (!s) {
        return NULL;
    }

    // 初始化内存
    memset(s, 0, sizeof(tcc_state_t));

    // 分配代码段
    s->code_capacity = 4096;  // 初始 4K
    s->code = tcc_mem_map(s->code_capacity, INFRA_MEM_PROT_READ | INFRA_MEM_PROT_WRITE);
    if (!s->code) {
        tcc_mem_free(s);
        return NULL;
    }

    // 分配数据段
    s->data_capacity = 4096;  // 初始 4K
    s->data = tcc_mem_map(s->data_capacity, INFRA_MEM_PROT_READ | INFRA_MEM_PROT_WRITE);
    if (!s->data) {
        tcc_mem_unmap(s->code, s->code_capacity);
        tcc_mem_free(s);
        return NULL;
    }

    return s;
}

// 删除 TCC 状态
void tcc_delete(tcc_state_t *s)
{
    if (!s) return;

    // 释放符号表
    if (s->symbols) {
        tcc_mem_free(s->symbols);
    }

    // 释放代码段
    if (s->code) {
        tcc_mem_unmap(s->code, s->code_capacity);
    }

    // 释放数据段
    if (s->data) {
        tcc_mem_unmap(s->data, s->data_capacity);
    }

    // 释放状态结构
    tcc_mem_free(s);
}

// 编译字符串
int tcc_compile_string(tcc_state_t *s, const char *str)
{
    if (!s || !str) {
        return -1;
    }

    // TODO: 实现编译功能
    return 0;
}

// 运行编译后的代码
int tcc_run(tcc_state_t *s, int argc, char **argv)
{
    if (!s || !s->code) {
        return -1;
    }

    // 设置代码段为可执行
    if (tcc_mem_protect(s->code, s->code_size, INFRA_MEM_PROT_READ | INFRA_MEM_PROT_EXEC) != 0) {
        return -1;
    }

    // 获取入口点
    typedef int (*entry_func_t)(int, char **);
    entry_func_t entry = (entry_func_t)s->code;

    // 执行代码
    return entry(argc, argv);
}

// 添加符号
int tcc_add_symbol(tcc_state_t *s, const char *name, const void *val)
{
    if (!s || !name || !val) {
        return -1;
    }

    // 重新分配符号表
    infra_symbol_t *new_symbols = tcc_mem_alloc((s->symbol_count + 1) * sizeof(infra_symbol_t));
    if (!new_symbols) {
        return -1;
    }

    // 复制现有符号
    if (s->symbols) {
        memcpy(new_symbols, s->symbols, s->symbol_count * sizeof(infra_symbol_t));
        tcc_mem_free(s->symbols);
    }

    // 添加新符号
    infra_symbol_t *sym = &new_symbols[s->symbol_count];
    sym->name = strdup(name);
    sym->addr = (void*)val;
    sym->size = 0;  // 未知大小
    sym->flags = 0;

    s->symbols = new_symbols;
    s->symbol_count++;

    return 0;
}

// 获取符号
void* tcc_get_symbol(tcc_state_t *s, const char *name)
{
    if (!s || !name) {
        return NULL;
    }

    // 查找符号
    for (size_t i = 0; i < s->symbol_count; i++) {
        if (strcmp(s->symbols[i].name, name) == 0) {
            return s->symbols[i].addr;
        }
    }

    return NULL;
}

// 获取错误消息
const char* tcc_get_error_msg(tcc_state_t *s)
{
    return s ? s->error_msg : "Invalid TCC state";
} 