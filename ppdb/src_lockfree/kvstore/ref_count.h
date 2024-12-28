#ifndef PPDB_REF_COUNT_H
#define PPDB_REF_COUNT_H

#include <cosmopolitan.h>

// 引用计数结构
typedef struct {
    atomic_uint count;         // 引用计数
    void* data;               // 数据指针
    void (*destructor)(void*); // 析构函数
} ref_count_t;

// 创建引用计数对象
ref_count_t* ref_count_create(void* data, void (*destructor)(void*));

// 增加引用计数
void ref_count_inc(ref_count_t* ref);

// 减少引用计数
void ref_count_dec(ref_count_t* ref);

// 获取当前引用计数值
uint32_t ref_count_get(ref_count_t* ref);

#endif // PPDB_REF_COUNT_H 