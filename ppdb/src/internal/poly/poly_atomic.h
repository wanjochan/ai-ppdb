#ifndef PPDB_POLY_ATOMIC_H
#define PPDB_POLY_ATOMIC_H

#include "internal/infra/infra_core.h"

//TODO later will migrate to infra_atomic.h/c

// 原子类型
typedef int32_t poly_atomic_t;

// 原子操作函数
int32_t poly_atomic_get(poly_atomic_t* v);
void poly_atomic_set(poly_atomic_t* v, int32_t n);
void poly_atomic_inc(poly_atomic_t* v);
void poly_atomic_dec(poly_atomic_t* v);
void poly_atomic_add(poly_atomic_t* v, int32_t n);
void poly_atomic_sub(poly_atomic_t* v, int32_t n);

#endif // PPDB_POLY_ATOMIC_H
