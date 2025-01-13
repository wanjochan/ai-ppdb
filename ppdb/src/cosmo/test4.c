#define _CRT_SECURE_NO_WARNINGS 1
#define WIN32_LEAN_AND_MEAN
#define _NO_CRT_STDIO_INLINE
#define __STDC_WANT_LIB_EXT1__ 1
#include "cosmopolitan.h"

/* 全局数据 */
static int counter = 0;

/* 内部函数 */
static int increment_counter(void) {
    return ++counter;
}

/* 导出函数 */
__attribute__((visibility("default")))
int test4_func(void) {
    counter = increment_counter();
    return counter;
} 