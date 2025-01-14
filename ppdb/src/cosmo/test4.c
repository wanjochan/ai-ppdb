#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT __attribute__((visibility("default")))
#endif

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
//#ifdef _WIN32
//__attribute__((section(".CRT$XCU")))
//static int DllMain(void* hinstDLL, unsigned long fdwReason, void* lpvReserved) {
//    switch (fdwReason) {
//        case 1: /* DLL_PROCESS_ATTACH */
//            counter = 0;
//            break;
//        case 0: /* DLL_PROCESS_DETACH */
//            break;
//        case 2: /* DLL_THREAD_ATTACH */
//            break;
//        case 3: /* DLL_THREAD_DETACH */
//            break;
//    }
//    return 1;
//}
//#endif 
