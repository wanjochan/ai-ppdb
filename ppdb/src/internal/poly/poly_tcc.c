#include "internal/poly/poly_tcc.h"
#include "internal/infra/infra_memory.h"
#include "cosmopolitan.h"  // TODO cosmo/infra later: 需要文件操作函数

// 内存管理函数
void* poly_tcc_malloc(size_t size)
{
    return infra_malloc(size);
}

void poly_tcc_free(void *ptr)
{
    infra_free(ptr);
}

infra_error_t poly_tcc_mmap(void *addr, size_t size, int prot)
{
    void *mem = mmap(addr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

infra_error_t poly_tcc_munmap(void *ptr, size_t size)
{
    if (munmap(ptr, size) != 0) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

infra_error_t poly_tcc_mprotect(void *ptr, size_t size, int prot)
{
    if (mprotect(ptr, size, prot) != 0) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

// TCC 状态管理
poly_tcc_state_t* poly_tcc_new(void)
{
    poly_tcc_state_t *s = poly_tcc_malloc(sizeof(poly_tcc_state_t));
    if (!s) {
        return NULL;
    }

    // 初始化内存
    infra_memset(s, 0, sizeof(poly_tcc_state_t));

    // 分配代码段
    s->code_capacity = 4096;  // 初始 4K
    s->code = poly_tcc_mmap(NULL, s->code_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->code) {
        poly_tcc_free(s);
        return NULL;
    }

    // 分配数据段
    s->data_capacity = 4096;  // 初始 4K
    s->data = poly_tcc_mmap(NULL, s->data_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->data) {
        poly_tcc_munmap(s->code, s->code_capacity);
        poly_tcc_free(s);
        return NULL;
    }

    // 初始化符号表
    s->symbol_capacity = 16;  // 初始容量
    s->symbol_names = poly_tcc_malloc(s->symbol_capacity * sizeof(char*));
    s->symbol_addrs = poly_tcc_malloc(s->symbol_capacity * sizeof(void*));
    if (!s->symbol_names || !s->symbol_addrs) {
        if (s->symbol_names) poly_tcc_free(s->symbol_names);
        if (s->symbol_addrs) poly_tcc_free(s->symbol_addrs);
        poly_tcc_munmap(s->data, s->data_capacity);
        poly_tcc_munmap(s->code, s->code_capacity);
        poly_tcc_free(s);
        return NULL;
    }

    return s;
}

void poly_tcc_delete(poly_tcc_state_t *s)
{
    if (!s) return;

    // 释放符号表
    if (s->symbol_names) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            if (s->symbol_names[i]) {
                poly_tcc_free(s->symbol_names[i]);
            }
        }
        poly_tcc_free(s->symbol_names);
    }
    if (s->symbol_addrs) {
        poly_tcc_free(s->symbol_addrs);
    }

    // 释放代码段和数据段
    if (s->code) {
        poly_tcc_munmap(s->code, s->code_capacity);
    }
    if (s->data) {
        poly_tcc_munmap(s->data, s->data_capacity);
    }

    // 释放状态结构
    poly_tcc_free(s);
}

// 编译和执行
int poly_tcc_compile_string(poly_tcc_state_t *s, const char *str)
{
    if (!s || !str) {
        infra_snprintf(s->error_msg, sizeof(s->error_msg), "Invalid parameters");
        return -1;
    }

    // 简单的机器码生成示例
    // 这里我们只生成一个简单的函数: return 42;
    unsigned char code[] = {
        0xb8, 0x2a, 0x00, 0x00, 0x00,  // mov eax, 42
        0xc3                            // ret
    };

    // 检查代码段容量
    if (s->code_size + sizeof(code) > s->code_capacity) {
        infra_snprintf(s->error_msg, sizeof(s->error_msg), "Code segment full");
        return -1;
    }

    // 复制代码到代码段
    infra_memcpy((char*)s->code + s->code_size, code, sizeof(code));
    s->code_size += sizeof(code);

    return 0;
}

int poly_tcc_run(poly_tcc_state_t *s, int argc, char **argv)
{
    if (!s || !s->code) {
        return -1;
    }

    // 设置代码段为可执行
    if (poly_tcc_mprotect(s->code, s->code_size, POLY_TCC_PROT_READ | POLY_TCC_PROT_EXEC) != 0) {
        return -1;
    }

    // 获取入口点
    typedef int (*entry_func_t)(int, char **);
    entry_func_t entry = (entry_func_t)s->code;

    // 执行代码
    return entry(argc, argv);
}

// 符号管理
int poly_tcc_add_symbol(poly_tcc_state_t *s, const char *name, const void *val)
{
    if (!s || !name || !val) {
        return -1;
    }

    // 检查是否需要扩容
    if (s->symbol_count >= s->symbol_capacity) {
        size_t new_capacity = s->symbol_capacity * 2;
        char **new_names = poly_tcc_malloc(new_capacity * sizeof(char*));
        void **new_addrs = poly_tcc_malloc(new_capacity * sizeof(void*));
        if (!new_names || !new_addrs) {
            if (new_names) poly_tcc_free(new_names);
            if (new_addrs) poly_tcc_free(new_addrs);
            return -1;
        }

        // 复制现有数据
        infra_memcpy(new_names, s->symbol_names, s->symbol_count * sizeof(char*));
        infra_memcpy(new_addrs, s->symbol_addrs, s->symbol_count * sizeof(void*));

        // 更新指针
        poly_tcc_free(s->symbol_names);
        poly_tcc_free(s->symbol_addrs);
        s->symbol_names = new_names;
        s->symbol_addrs = new_addrs;
        s->symbol_capacity = new_capacity;
    }

    // 添加新符号 - 替换 _strdup
    size_t name_len = infra_strlen(name) + 1;
    s->symbol_names[s->symbol_count] = poly_tcc_malloc(name_len);
    if (!s->symbol_names[s->symbol_count]) {
        return -1;
    }
    infra_strcpy(s->symbol_names[s->symbol_count], name);
    s->symbol_addrs[s->symbol_count] = (void*)val;
    s->symbol_count++;

    return 0;
}

void* poly_tcc_get_symbol(poly_tcc_state_t *s, const char *name)
{
    if (!s || !name) {
        return NULL;
    }

    // 查找符号
    for (size_t i = 0; i < s->symbol_count; i++) {
        if (infra_strcmp(s->symbol_names[i], name) == 0) {
            return s->symbol_addrs[i];
        }
    }

    return NULL;
}

// 错误处理
const char* poly_tcc_get_error_msg(poly_tcc_state_t *s)
{
    return s ? s->error_msg : "Invalid TCC state";
}

// 新增: 符号管理 API 实现
infra_error_t poly_sym_lookup(const char* name, void** addr)
{
    if (!name || !addr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO cosmo/infra later: 使用 infra 层的符号查找函数
    *addr = dlsym(RTLD_DEFAULT, name);
    if (!*addr) {
        return INFRA_ERROR_NOT_FOUND;
    }
    return INFRA_OK;
}

infra_error_t poly_sym_add(const char* name, void* addr)
{
    if (!name || !addr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO cosmo/infra later: 使用 infra 层的符号添加函数
    return INFRA_ERROR_NOT_SUPPORTED;
}

infra_error_t poly_sym_remove(const char* name)
{
    if (!name) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // TODO cosmo/infra later: 使用 infra 层的符号删除函数
    return INFRA_ERROR_NOT_SUPPORTED;
}

// 新增: 内存管理 API 实现
infra_error_t poly_mem_exec(void* ptr, size_t size)
{
    if (!ptr || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    return infra_mem_protect(ptr, size, POLY_TCC_PROT_READ | POLY_TCC_PROT_EXEC);
}

infra_error_t poly_mem_map(size_t size, void **ptr)
{
    return infra_mem_map(NULL, size, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
}

infra_error_t poly_mem_unmap(void* ptr, size_t size)
{
    if (!ptr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    return infra_mem_unmap(ptr, size);
} 