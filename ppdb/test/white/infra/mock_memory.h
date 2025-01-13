#ifndef MOCK_MEMORY_H
#define MOCK_MEMORY_H

#include "infra/infra_core.h"

// Memory allocation functions
void* mock_malloc(size_t size);
void* mock_calloc(size_t nmemb, size_t size);
void* mock_realloc(void* ptr, size_t size);
void mock_free(void* ptr);

// Memory manipulation functions
void* mock_memset(void* s, int c, size_t n);
void* mock_memcpy(void* dest, const void* src, size_t n);
void* mock_memmove(void* dest, const void* src, size_t n);

#endif /* MOCK_MEMORY_H */