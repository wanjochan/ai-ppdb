#include <cosmopolitan.h>
#include "ppdb/ref_count.h"

// 创建引用计数对象
ref_count_t* ref_count_create(void* data, void (*destructor)(void*)) {
    ref_count_t* ref = (ref_count_t*)malloc(sizeof(ref_count_t));
    if (!ref) return NULL;

    atomic_init(&ref->count, 1);
    ref->data = data;
    ref->destructor = destructor;
    return ref;
}

// 增加引用计数
void ref_count_inc(ref_count_t* ref) {
    if (!ref) return;
    atomic_fetch_add_explicit(&ref->count, 1, memory_order_relaxed);
}

// 减少引用计数
void ref_count_dec(ref_count_t* ref) {
    if (!ref) return;
    
    if (atomic_fetch_sub_explicit(&ref->count, 1, memory_order_release) == 1) {
        atomic_thread_fence(memory_order_acquire);
        if (ref->destructor) {
            ref->destructor(ref->data);
        }
        free(ref);
    }
}

// 获取当前引用计数值
uint32_t ref_count_get(ref_count_t* ref) {
    if (!ref) return 0;
    return atomic_load_explicit(&ref->count, memory_order_relaxed);
} 