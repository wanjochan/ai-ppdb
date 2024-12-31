#ifndef _RUNTIME_H_
#define _RUNTIME_H_

// 基本类型定义
typedef unsigned long size_t;
typedef int bool;
#define true 1
#define false 0
#define NULL ((void*)0)

// 字符串函数声明
size_t strlen(const char* str);
void* memcpy(void* dest, const void* src, size_t n);

// 输出函数声明
int puts(const char* str);
int printf(const char* format, ...);

#endif // _RUNTIME_H_ 