#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* 导出宏定义 */
#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __attribute__((visibility("default")))
#endif

/* 版本信息 */
#define DL_VERSION_MAJOR 1
#define DL_VERSION_MINOR 0
#define DL_VERSION_PATCH 0

/* 全局数据 */
static int counter = 0;

/* 内部函数 */
static int increment_counter(void) {
    return ++counter;
}

/* 导出函数 */
DLL_EXPORT
int test4_func(void) {
    counter = increment_counter();
    return counter;
}

/* Windows DLL入口点 */
#ifdef _WIN32
__attribute__((section(".CRT$XCU")))
static int DllMain(void* hinstDLL, unsigned long fdwReason, void* lpvReserved) {
    switch (fdwReason) {
        case 1: /* DLL_PROCESS_ATTACH */
            counter = 0;
            break;
        case 0: /* DLL_PROCESS_DETACH */
            break;
        case 2: /* DLL_THREAD_ATTACH */
            break;
        case 3: /* DLL_THREAD_DETACH */
            break;
    }
    return 1;
}
#endif

/* Linux构造/析构函数 */
#ifdef __linux__
__attribute__((constructor))
static void dl_init(void) {
    counter = 0;
}

__attribute__((destructor))
static void dl_fini(void) {
}
#endif

/* macOS版本信息 */
#ifdef __APPLE__
/* 当前版本 */
__attribute__((section("__TEXT,__const")))
static const struct {
    unsigned long version;
    unsigned long compat;
} dl_macos_version = {
    .version = DL_VERSION_MAJOR << 16 | DL_VERSION_MINOR << 8 | DL_VERSION_PATCH,
    .compat = 0
};

/* 构造/析构函数 */
__attribute__((constructor))
static void dl_init(void) {
    counter = 0;
}

__attribute__((destructor))
static void dl_fini(void) {
}
#endif

/* 版本信息符号（Linux） */
#ifdef __linux__
__asm__(".symver test4_func,test4_func@VERS_1.0");
__attribute__((section(".gnu.version_d")))
static const struct {
    unsigned short version;
    unsigned short flags;
    unsigned short ndx;
    unsigned short cnt;
    unsigned short name;
    unsigned short aux;
} dl_version_info = {
    .version = 1,
    .flags = 0,
    .ndx = 2,
    .cnt = 1,
    .name = DL_VERSION_MAJOR << 8 | DL_VERSION_MINOR,
    .aux = 0
};
#endif 