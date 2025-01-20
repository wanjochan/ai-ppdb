#include "internal/poly/poly_tcc.h"

// 内存管理函数
void* poly_tcc_malloc(size_t size)
{
    return malloc(size);
}

void poly_tcc_free(void *ptr)
{
    free(ptr);
}

void* poly_tcc_mmap(size_t size, int prot)
{
    DWORD flProtect = PAGE_NOACCESS;
    if (prot & POLY_TCC_PROT_READ) {
        if (prot & POLY_TCC_PROT_WRITE) {
            flProtect = PAGE_READWRITE;
        } else {
            flProtect = PAGE_READONLY;
        }
    }
    if (prot & POLY_TCC_PROT_EXEC) {
        if (prot & POLY_TCC_PROT_WRITE) {
            flProtect = PAGE_EXECUTE_READWRITE;
        } else {
            flProtect = PAGE_EXECUTE_READ;
        }
    }

    void *mem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, flProtect);
    if (!mem) {
        return NULL;
    }

    return mem;
}

int poly_tcc_munmap(void *ptr, size_t size)
{
    return VirtualFree(ptr, 0, MEM_RELEASE) ? 0 : -1;
}

int poly_tcc_mprotect(void *ptr, size_t size, int prot)
{
    DWORD flNewProtect = PAGE_NOACCESS;
    if (prot & POLY_TCC_PROT_READ) {
        if (prot & POLY_TCC_PROT_WRITE) {
            flNewProtect = PAGE_READWRITE;
        } else {
            flNewProtect = PAGE_READONLY;
        }
    }
    if (prot & POLY_TCC_PROT_EXEC) {
        if (prot & POLY_TCC_PROT_WRITE) {
            flNewProtect = PAGE_EXECUTE_READWRITE;
        } else {
            flNewProtect = PAGE_EXECUTE_READ;
        }
    }

    DWORD flOldProtect;
    return VirtualProtect(ptr, size, flNewProtect, &flOldProtect) ? 0 : -1;
}

// TCC 状态管理
poly_tcc_state_t* poly_tcc_new(void)
{
    poly_tcc_state_t *s = poly_tcc_malloc(sizeof(poly_tcc_state_t));
    if (!s) {
        return NULL;
    }

    // 初始化内存
    memset(s, 0, sizeof(poly_tcc_state_t));

    // 分配代码段
    s->code_capacity = 4096;  // 初始 4K
    s->code = poly_tcc_mmap(s->code_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->code) {
        poly_tcc_free(s);
        return NULL;
    }

    // 分配数据段
    s->data_capacity = 4096;  // 初始 4K
    s->data = poly_tcc_mmap(s->data_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
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
        return -1;
    }

    // TODO: 实现编译功能
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
        memcpy(new_names, s->symbol_names, s->symbol_count * sizeof(char*));
        memcpy(new_addrs, s->symbol_addrs, s->symbol_count * sizeof(void*));

        // 更新指针
        poly_tcc_free(s->symbol_names);
        poly_tcc_free(s->symbol_addrs);
        s->symbol_names = new_names;
        s->symbol_addrs = new_addrs;
        s->symbol_capacity = new_capacity;
    }

    // 添加新符号
    s->symbol_names[s->symbol_count] = _strdup(name);
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
        if (strcmp(s->symbol_names[i], name) == 0) {
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

    HMODULE hModule = GetModuleHandle(NULL);
    *addr = (void*)GetProcAddress(hModule, name);
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

    // Windows 不支持动态添加符号，这里返回不支持
    return INFRA_ERROR_NOT_SUPPORTED;
}

infra_error_t poly_sym_remove(const char* name)
{
    if (!name) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Windows 不支持动态删除符号，这里返回不支持
    return INFRA_ERROR_NOT_SUPPORTED;
}

// 新增: 内存管理 API 实现
infra_error_t poly_mem_exec(void* ptr, size_t size)
{
    if (!ptr || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    DWORD oldProtect;
    if (!VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &oldProtect)) {
        return INFRA_ERROR_MEMORY;
    }

    return INFRA_OK;
}

infra_error_t poly_mem_map(size_t size, void** ptr)
{
    if (!ptr || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!*ptr) {
        return INFRA_ERROR_MEMORY;
    }

    return INFRA_OK;
}

infra_error_t poly_mem_unmap(void* ptr, size_t size)
{
    if (!ptr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!VirtualFree(ptr, 0, MEM_RELEASE)) {
        return INFRA_ERROR_MEMORY;
    }

    return INFRA_OK;
} 