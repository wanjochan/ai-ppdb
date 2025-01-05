/*
 * base_utils.inc.c - Core utility functions for PPDB base layer
 *
 * This file contains common utility functions used throughout the PPDB base layer,
 * including string handling, memory management, and basic data structure operations.
 *
 * Copyright (c) 2023 PPDB Authors
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Standardized error handling macros
#define PPDB_BASE_CHECK_NULL(ptr) if ((ptr) == NULL) return PPDB_ERR_PARAM
#define PPDB_BASE_CHECK_PARAM(cond) if (!(cond)) return PPDB_ERR_PARAM

// String utility functions
ppdb_error_t ppdb_base_init_string(char **dest, const char *src) {
    PPDB_BASE_CHECK_NULL(dest);
    PPDB_BASE_CHECK_NULL(src);
    
    size_t len = strlen(src);
    *dest = ppdb_base_aligned_alloc(1, len + 1);
    if (!*dest) return PPDB_ERR_MEMORY;
    
    memcpy(*dest, src, len + 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_copy_buffer(void *dest, const void *src, size_t len) {
    PPDB_BASE_CHECK_NULL(dest);
    PPDB_BASE_CHECK_NULL(src);
    
    memcpy(dest, src, len);
    return PPDB_OK;
}

// Utils initialization
ppdb_error_t ppdb_base_utils_init(ppdb_base_t* base) {
    if (!base) return PPDB_ERR_PARAM;
    return PPDB_OK;
}

void ppdb_base_utils_cleanup(ppdb_base_t* base) {
    if (!base) return;
}

// ... additional utility functions with standardized naming and error handling ...