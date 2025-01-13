#include "ape_loader.h"

/* 平台特定的加载函数声明 */
static void* platform_load(const char* path);
static void* platform_get_proc(void* handle, const char* symbol);
static int platform_unload(void* handle);

/* 禁用Windows错误弹窗 */
static void disable_error_dialogs(void) {
#if defined(__COSMOPOLITAN__)
    /* 设置Windows错误模式 */
    SetErrorMode(SEM_FAILCRITICALERRORS | 
                SEM_NOGPFAULTERRORBOX | 
                SEM_NOOPENFILEERRORBOX);
#endif
}

/* 平台特定的实现 */
#if defined(__COSMOPOLITAN__)
/* Cosmopolitan 环境下使用 cosmo_dlxxx 函数 */
static void* platform_load(const char* path) {
    disable_error_dialogs();
    return cosmo_dlopen(path, RTLD_NOW);
}
static void* platform_get_proc(void* handle, const char* symbol) {
    return cosmo_dlsym(handle, symbol);
}
static int platform_unload(void* handle) {
    return cosmo_dlclose(handle);
}
#else
#error "Unsupported platform - must be built with Cosmopolitan"
#endif

/* APE 加载器实现 */
void* ape_load(const char* path) {
    void* handle = platform_load(path);
    if (!handle) {
        return NULL;
    }
    
    /* 验证APE头部 */
    struct ApeHeader* header = (struct ApeHeader*)handle;
    if (header->mz_magic != 0x5A4D || 
        header->pe_magic != 0x4550 ||
        header->elf_magic != 0x464C457F ||
        header->macho_magic != 0xFEEDFACF) {
        platform_unload(handle);
        return NULL;
    }
    
    return handle;
}

void* ape_get_proc(void* handle, const char* symbol) {
    return platform_get_proc(handle, symbol);
}

int ape_unload(void* handle) {
    return platform_unload(handle);
} 