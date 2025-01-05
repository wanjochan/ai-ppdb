/*
 * base_utils.inc.c - Core utility functions for PPDB base layer
 *
 * This file contains common utility functions used throughout the PPDB base layer,
 * including string handling, memory management, and basic data structure operations.
 *
 * Copyright (c) 2023 PPDB Authors
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Standardized error handling macros
#define PPDB_BASE_CHECK_NULL(ptr) if ((ptr) == NULL) return PPDB_ERROR_NULL_PTR
#define PPDB_BASE_CHECK_PARAM(cond) if (!(cond)) return PPDB_ERROR_INVALID_PARAM

// Rename utility functions with ppdb_base_ prefix
int ppdb_base_init_string(char **dest, const char *src) {
    PPDB_BASE_CHECK_NULL(dest);
    PPDB_BASE_CHECK_NULL(src);
    
    // ... existing implementation ...
}

int ppdb_base_copy_buffer(void *dest, const void *src, size_t len) {
    PPDB_BASE_CHECK_NULL(dest);
    PPDB_BASE_CHECK_NULL(src);
    
    // ... existing implementation ...
}

// ... additional utility functions with standardized naming and error handling ...