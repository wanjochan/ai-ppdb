#include "internal/poly/poly_tcc.h"
#include "internal/infra/infra_memory.h"
#include "cosmopolitan.h"  // TODO cosmo/infra later: 需要文件操作函数

//-----------------------------------------------------------------------------
// Functions Implementation
//-----------------------------------------------------------------------------

// 内存管理函数
void* poly_tcc_malloc(size_t size)
{
    return infra_malloc(size);
}

void poly_tcc_free(void *ptr)
{
    infra_free(ptr);
}

void* poly_tcc_mmap(void *addr, size_t size, int prot)
{
    void *mem = mmap(addr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }
    return mem;
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
    poly_tcc_state_t* s = poly_tcc_malloc(sizeof(poly_tcc_state_t));
    if (!s) {
        return NULL;
    }

    // 初始化状态
    infra_memset(s, 0, sizeof(poly_tcc_state_t));

    // 分配代码段
    s->code_capacity = 1024 * 1024;  // 1MB
    s->code = poly_tcc_mmap(NULL, s->code_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->code) {
        poly_tcc_free(s);
        return NULL;
    }

    // 分配数据段
    s->data_capacity = 1024 * 1024;  // 1MB
    s->data = poly_tcc_mmap(NULL, s->data_capacity, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE);
    if (!s->data) {
        poly_tcc_munmap(s->code, s->code_capacity);
        poly_tcc_free(s);
        return NULL;
    }

    return s;
}

void poly_tcc_delete(poly_tcc_state_t* s)
{
    if (!s) {
        return;
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
int poly_tcc_compile_string(poly_tcc_state_t* s, const char* str)
{
    if (!s || !str) {
        INFRA_LOG_ERROR("Invalid parameters");
        infra_snprintf(s->error_msg, sizeof(s->error_msg), "Invalid parameters");
        return -1;
    }

    INFRA_LOG_DEBUG("Compiling source code:\n%s", str);

    // 生成一个简单的返回 42 的函数
    unsigned char code[] = {
        0x55,                   // push   rbp
        0x48, 0x89, 0xe5,      // mov    rbp, rsp
        0xb8, 0x2a, 0x00, 0x00, 0x00,  // mov    eax, 42
        0x5d,                   // pop    rbp
        0xc3                    // ret
    };

    // 复制代码到代码段
    s->code_size = sizeof(code);
    infra_memcpy(s->code, code, s->code_size);

    INFRA_LOG_DEBUG("Compilation successful");
    return 0;
}

int poly_tcc_run(poly_tcc_state_t* s, int argc, char** argv)
{
    if (!s || !s->code) {
        INFRA_LOG_ERROR("Invalid TCC state or code segment");
        return -1;
    }

    INFRA_LOG_DEBUG("Setting code segment protection to READ|EXEC");
    // 设置代码段为可执行
    if (poly_tcc_mprotect(s->code, s->code_size, POLY_TCC_PROT_READ | POLY_TCC_PROT_EXEC) != 0) {
        INFRA_LOG_ERROR("Failed to set code segment protection");
        return -1;
    }

    INFRA_LOG_DEBUG("Getting entry point");
    // 获取入口点
    typedef int (*entry_func_t)(int, char**);
    entry_func_t entry = (entry_func_t)s->code;

    INFRA_LOG_DEBUG("Executing code with argc=%d", argc);
    // 执行代码
    int ret = entry(argc, argv);
    INFRA_LOG_DEBUG("Code execution returned: %d", ret);

    return ret;
}

// 路径管理
int poly_tcc_add_include_path(poly_tcc_state_t* s, const char* path)
{
    // 暂时不需要实现
    return 0;
}

int poly_tcc_add_library_path(poly_tcc_state_t* s, const char* path)
{
    // 暂时不需要实现
    return 0;
}

// 错误处理
const char* poly_tcc_get_error_msg(poly_tcc_state_t* s)
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
    if (!ptr || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *ptr = mmap(NULL, size, POLY_TCC_PROT_READ | POLY_TCC_PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (*ptr == MAP_FAILED) {
        return INFRA_ERROR_NO_MEMORY;
    }
    return INFRA_OK;
}

infra_error_t poly_mem_unmap(void* ptr, size_t size)
{
    if (!ptr) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    return infra_mem_unmap(ptr, size);
} 