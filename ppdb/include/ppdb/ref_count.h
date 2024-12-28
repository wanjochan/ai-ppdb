#ifndef PPDB_REF_COUNT_H
#define PPDB_REF_COUNT_H

#include <cosmopolitan.h>

// 释放回调函数类型
typedef void (*ref_count_free_fn)(void* ptr);

// 引用计数结构
typedef struct ref_count_t {
    atomic_uint count;          // 引用计数
    void* data;                // 数据指针
    ref_count_free_fn destructor; // 析构函数
} ref_count_t;

// 创建引用计数
ref_count_t* ref_count_create(void* ptr, ref_count_free_fn free_fn);

// 增加引用计数
void ref_count_inc(ref_count_t* ref);

// 减少引用计数，如果计数为0则释放对象
void ref_count_dec(ref_count_t* ref);

// 获取当前引用计数值
uint32_t ref_count_get(ref_count_t* ref);

#endif // PPDB_REF_COUNT_H 