/*
 * infra_memory.h - Memory Management Module
 */

#ifndef INFRA_MEMORY_H
#define INFRA_MEMORY_H

#include "cosmopolitan.h"
#include "internal/infra/infra_error.h"

//-----------------------------------------------------------------------------
// Memory Configuration
//-----------------------------------------------------------------------------

typedef struct {
    bool use_memory_pool;
    size_t pool_initial_size;
    size_t pool_alignment;
} infra_memory_config_t;

//-----------------------------------------------------------------------------
// Memory Statistics
//-----------------------------------------------------------------------------

typedef struct {
    size_t current_usage;
    size_t peak_usage;
    size_t total_allocations;
} infra_memory_stats_t;

//-----------------------------------------------------------------------------
// Memory Management Functions
//-----------------------------------------------------------------------------

void* infra_malloc(size_t size);
void* infra_calloc(size_t nmemb, size_t size);
void* infra_realloc(void* ptr, size_t size);
void infra_free(void* ptr);

//-----------------------------------------------------------------------------
// Memory Operations
//-----------------------------------------------------------------------------

void* infra_memset(void* s, int c, size_t n);
void* infra_memcpy(void* dest, const void* src, size_t n);
void* infra_memmove(void* dest, const void* src, size_t n);
int infra_memcmp(const void* s1, const void* s2, size_t n);

//-----------------------------------------------------------------------------
// Memory Module Management
//-----------------------------------------------------------------------------

infra_error_t infra_memory_init(const infra_memory_config_t* config);
void infra_memory_cleanup(void);
infra_error_t infra_memory_get_stats(infra_memory_stats_t* stats);

#endif // INFRA_MEMORY_H 