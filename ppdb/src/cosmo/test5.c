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
static char message[256] = "Hello from test5";

/* 内部函数 */
static int increment_counter(void) {
    return ++counter;
}

/* 导出函数 - 整数返回值 */
DLL_EXPORT
int test5_func_int(void) {
    counter = increment_counter();
    return counter;
}

/* 导出函数 - 字符串返回值 */
DLL_EXPORT
const char* test5_func_str(void) {
    return message;
}

/* 导出函数 - 打印消息 */
DLL_EXPORT
void test5_func_print(void) {
    dprintf(1, "Counter: %d, Message: %s\n", counter, message);
} 