#ifndef MOCK_CORE_H
#define MOCK_CORE_H

#include "internal/infra/infra_core.h"
#include "test/white/framework/mock_framework.h"

// Mock functions for memory operations
void* mock_malloc(size_t size);
void* mock_calloc(size_t nmemb, size_t size);
void* mock_realloc(void* ptr, size_t size);
void mock_free(void* ptr);
void* mock_memset(void* s, int c, size_t n);
void* mock_memcpy(void* dest, const void* src, size_t n);
void* mock_memmove(void* dest, const void* src, size_t n);

// Mock functions for string operations
size_t mock_strlen(const char* s);
char* mock_strcpy(char* dest, const char* src);
char* mock_strncpy(char* dest, const char* src, size_t n);
char* mock_strcat(char* dest, const char* src);
char* mock_strncat(char* dest, const char* src, size_t n);
int mock_strcmp(const char* s1, const char* s2);
int mock_strncmp(const char* s1, const char* s2, size_t n);
char* mock_strdup(const char* s);

// Mock functions for time operations
infra_time_t mock_time_now(void);
infra_time_t mock_time_monotonic(void);
void mock_time_sleep(uint32_t ms);
void mock_time_yield(void);

// Mock functions for logging
void mock_log(int level, const char* file, int line, const char* func, const char* format, ...);
void mock_log_set_level(int level);

// Mock control functions
void mock_core_init(void);
void mock_core_verify(void);
void mock_core_cleanup(void);

#endif /* MOCK_CORE_H */ 