/*
 * base_core.inc.c - Core Infrastructure Implementation
 *
 * This file contains:
 * 1. Basic data structures
 * 2. Core utilities
 * 3. System information
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Core utility functions
bool ppdb_base_is_power_of_two(size_t x) {
    return x && !(x & (x - 1));
}

size_t ppdb_base_align_size(size_t size, size_t alignment) {
    return (size + (alignment - 1)) & ~(alignment - 1);
}

bool ppdb_base_str_equal(const char* s1, const char* s2) {
    return strcmp(s1, s2) == 0;
}

size_t ppdb_base_str_hash(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Time and system functions
uint64_t ppdb_base_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

// Comparison functions
int ppdb_base_ptr_compare(const void* a, const void* b) {
    return (a > b) - (a < b);
}

int ppdb_base_int_compare(const void* a, const void* b) {
    const int* ia = (const int*)a;
    const int* ib = (const int*)b;
    return (*ia > *ib) - (*ia < *ib);
}

int ppdb_base_str_compare(const void* a, const void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    return strcmp(sa, sb);
}

// Path handling
void ppdb_base_normalize_path(char* path) {
    char* src = path;
    char* dst = path;
    
    while (*src) {
        if (*src == '\\') {
            *dst++ = '/';
            src++;
            while (*src == '\\' || *src == '/') src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

bool ppdb_base_is_absolute_path(const char* path) {
    if (!path) return false;
    #ifdef _WIN32
    if (isalpha(path[0]) && path[1] == ':' && 
        (path[2] == '\\' || path[2] == '/')) {
        return true;
    }
    #endif
    return path[0] == '/' || path[0] == '\\';
}

void ppdb_base_get_dirname(char* path) {
    char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        last_slash = strrchr(path, '\\');
    }
    if (last_slash) {
        *last_slash = '\0';
    }
}

void ppdb_base_get_basename(const char* path, char* basename, size_t size) {
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) {
        last_slash = strrchr(path, '\\');
    }
    if (last_slash) {
        strlcpy(basename, last_slash + 1, size);
    } else {
        strlcpy(basename, path, size);
    }
}

// System information
uint32_t ppdb_base_get_cpu_count(void) {
    #ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
    #else
    return sysconf(_SC_NPROCESSORS_ONLN);
    #endif
}

size_t ppdb_base_get_page_size(void) {
    #ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwPageSize;
    #else
    return sysconf(_SC_PAGESIZE);
    #endif
}

uint64_t ppdb_base_get_total_memory(void) {
    #ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys;
    #else
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return si.totalram * si.mem_unit;
    }
    return 0;
    #endif
} 