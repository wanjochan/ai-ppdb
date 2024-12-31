#include "runtime.h"

// Windows API 函数声明
__declspec(dllimport) int __stdcall WriteConsoleA(void* hConsoleOutput, const char* lpBuffer, unsigned long nNumberOfCharsToWrite, unsigned long* lpNumberOfCharsWritten, void* lpReserved);
__declspec(dllimport) void* __stdcall GetStdHandle(unsigned long nStdHandle);

#define STD_OUTPUT_HANDLE ((unsigned long)-11)

// 字符串函数实现
size_t strlen(const char* str) {
    const char* s;
    for (s = str; *s; ++s);
    return s - str;
}

void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

// 输出函数实现
int puts(const char* str) {
    void* handle = GetStdHandle(STD_OUTPUT_HANDLE);
    unsigned long written;
    size_t len = strlen(str);
    
    if (WriteConsoleA(handle, str, len, &written, NULL)) {
        // 添加换行符
        WriteConsoleA(handle, "\n", 1, &written, NULL);
        return 0;
    }
    return -1;
}

// 简单的 printf 实现，只支持 %s 和 %d
int printf(const char* format, ...) {
    char buffer[1024];
    char* p = buffer;
    char numbuf[32];
    void** args = (void**)&format + 1;
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 's': {
                    const char* s = (const char*)*args++;
                    if (!s) s = "(null)";
                    while (*s) *p++ = *s++;
                    break;
                }
                case 'd': {
                    int num = *(int*)args++;
                    char* numstr = numbuf;
                    int neg = 0;
                    
                    if (num < 0) {
                        neg = 1;
                        num = -num;
                    }
                    
                    do {
                        *numstr++ = '0' + (num % 10);
                        num /= 10;
                    } while (num);
                    
                    if (neg) *p++ = '-';
                    
                    while (numstr > numbuf) {
                        *p++ = *--numstr;
                    }
                    break;
                }
                default:
                    *p++ = *format;
            }
        } else {
            *p++ = *format;
        }
        format++;
    }
    *p = '\0';
    
    return puts(buffer);
} 