#include "cosmopolitan.h"

/* 错误信息缓冲区 */
static char error_buffer[1024];
static char* last_error = NULL;

/* 设置错误信息 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(error_buffer, sizeof(error_buffer), format, args);
    va_end(args);
    last_error = error_buffer;
}

/* 清除错误信息 */
static void clear_error(void) {
    last_error = NULL;
}

/* 加载动态库 */
void* cosmo_dlopen(const char* filename, int flags) {
    void* handle = NULL;
    clear_error();

    /* 检查文件是否存在 */
    if (access(filename, F_OK) != 0) {
        set_error("File not found: %s", filename);
        return NULL;
    }

    /* 尝试加载文件 */
#if defined(__COSMOPOLITAN__)
    if (IsWindows()) {
        handle = LoadLibraryA(filename);
        if (!handle) {
            set_error("LoadLibrary failed: %lu", GetLastError());
        }
    } else if (IsLinux()) {
        handle = dlopen(filename, flags);
        if (!handle) {
            set_error("dlopen failed: %s", dlerror());
        }
    } else if (IsMacho()) {
        /* TODO: 实现 macOS 支持 */
        set_error("macOS support not implemented yet");
    } else {
        set_error("Unsupported platform");
    }
#else
    set_error("Not built with Cosmopolitan");
#endif

    return handle;
}

/* 获取符号地址 */
void* cosmo_dlsym(void* handle, const char* symbol) {
    void* addr = NULL;
    clear_error();

    if (!handle) {
        set_error("Invalid handle");
        return NULL;
    }

    if (!symbol) {
        set_error("Invalid symbol name");
        return NULL;
    }

#if defined(__COSMOPOLITAN__)
    if (IsWindows()) {
        addr = GetProcAddress(handle, symbol);
        if (!addr) {
            set_error("GetProcAddress failed: %lu", GetLastError());
        }
    } else if (IsLinux()) {
        addr = dlsym(handle, symbol);
        if (!addr) {
            set_error("dlsym failed: %s", dlerror());
        }
    } else if (IsMacho()) {
        /* TODO: 实现 macOS 支持 */
        set_error("macOS support not implemented yet");
    } else {
        set_error("Unsupported platform");
    }
#else
    set_error("Not built with Cosmopolitan");
#endif

    return addr;
}

/* 卸载动态库 */
int cosmo_dlclose(void* handle) {
    int result = -1;
    clear_error();

    if (!handle) {
        set_error("Invalid handle");
        return result;
    }

#if defined(__COSMOPOLITAN__)
    if (IsWindows()) {
        if (FreeLibrary(handle)) {
            result = 0;
        } else {
            set_error("FreeLibrary failed: %lu", GetLastError());
        }
    } else if (IsLinux()) {
        result = dlclose(handle);
        if (result != 0) {
            set_error("dlclose failed: %s", dlerror());
        }
    } else if (IsMacho()) {
        /* TODO: 实现 macOS 支持 */
        set_error("macOS support not implemented yet");
    } else {
        set_error("Unsupported platform");
    }
#else
    set_error("Not built with Cosmopolitan");
#endif

    return result;
}

/* 获取最后的错误信息 */
const char* cosmo_dlerror(void) {
    return last_error;
} 