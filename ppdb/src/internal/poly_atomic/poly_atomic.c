#include "poly_atomic.h"
#include <stdatomic.h>

int32_t poly_atomic_get(poly_atomic_t* v) {
    return atomic_load((atomic_int*)v);
}

void poly_atomic_set(poly_atomic_t* v, int32_t n) {
    atomic_store((atomic_int*)v, n);
}

void poly_atomic_inc(poly_atomic_t* v) {
    atomic_fetch_add((atomic_int*)v, 1);
}

void poly_atomic_dec(poly_atomic_t* v) {
    atomic_fetch_sub((atomic_int*)v, 1);
}

void poly_atomic_add(poly_atomic_t* v, int32_t n) {
    atomic_fetch_add((atomic_int*)v, n);
}

void poly_atomic_sub(poly_atomic_t* v, int32_t n) {
    atomic_fetch_sub((atomic_int*)v, n);
}
