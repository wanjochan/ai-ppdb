#include "ape_loader.h"

/* Windows错误模式常量 */
#define APE_SEM_FAILCRITICALERRORS     0x0001
#define APE_SEM_NOALIGNMENTFAULTEXCEPT 0x0004
#define APE_SEM_NOGPFAULTERRORBOX      0x0002
#define APE_SEM_NOOPENFILEERRORBOX     0x8000

/* 平台特定的加载函数声明 */
static void* platform_load(const char* path);
static void* platform_get_proc(void* handle, const char* symbol);
static int platform_unload(void* handle);

/* 禁用Windows错误弹窗 */
static void disable_error_dialogs(void) {
#if defined(__COSMOPOLITAN__)
    /* 设置Windows错误模式 */
    SetErrorMode(APE_SEM_FAILCRITICALERRORS | 
                APE_SEM_NOGPFAULTERRORBOX | 
                APE_SEM_NOOPENFILEERRORBOX);
#endif
}

/* 平台检测函数 */
static bool IsMacho(void) {
#if defined(__COSMOPOLITAN__)
    return IsXnu();
#else
    return false;
#endif
}

/* 平台特定的实现 */
#if defined(__COSMOPOLITAN__)
static void* platform_load(const char* path) {
    disable_error_dialogs();
    if (IsWindows()) {
        return (void*)(intptr_t)LoadLibraryA(path);
    } else if (IsLinux()) {
        return dlopen(path, RTLD_NOW);
    } else if (IsMacho()) {
        /* TODO: 实现 macOS 支持 */
        return NULL;
    }
    return NULL;
}

static void* platform_get_proc(void* handle, const char* symbol) {
    if (IsWindows()) {
        return (void*)(intptr_t)GetProcAddress((int64_t)(intptr_t)handle, symbol);
    } else if (IsLinux()) {
        return dlsym(handle, symbol);
    } else if (IsMacho()) {
        /* TODO: 实现 macOS 支持 */
        return NULL;
    }
    return NULL;
}

static int platform_unload(void* handle) {
    if (IsWindows()) {
        return FreeLibrary((int64_t)(intptr_t)handle) ? 0 : -1;
    } else if (IsLinux()) {
        return dlclose(handle);
    } else if (IsMacho()) {
        /* TODO: 实现 macOS 支持 */
        return -1;
    }
    return -1;
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