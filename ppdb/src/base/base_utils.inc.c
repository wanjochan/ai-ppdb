/*
 * base_utils.inc.c - Utility Functions Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// String operations
bool ppdb_base_str_equal(const char* s1, const char* s2) {
    if (!s1 || !s2) return false;
    return strcmp(s1, s2) == 0;
}

size_t ppdb_base_str_hash(const char* str) {
    if (!str) return 0;

    size_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

// Time functions
uint64_t ppdb_base_get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

// Comparison functions
int ppdb_base_ptr_compare(const void* a, const void* b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

int ppdb_base_int_compare(const void* a, const void* b) {
    int va = *(const int*)a;
    int vb = *(const int*)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

int ppdb_base_str_compare(const void* a, const void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    if (!sa && !sb) return 0;
    if (!sa) return -1;
    if (!sb) return 1;
    return strcmp(sa, sb);
}

// Path utilities
void ppdb_base_normalize_path(char* path) {
    if (!path) return;

    char* p;
    size_t len = strlen(path);

    // Convert backslashes to forward slashes
    for (p = path; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    // Remove trailing slashes
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = '\0';
    }
}

bool ppdb_base_is_absolute_path(const char* path) {
    if (!path) return false;
    
#ifdef _WIN32
    // Check for drive letter (e.g., "C:/") or UNC path ("//")
    if (isalpha(path[0]) && path[1] == ':') return true;
    if (path[0] == '/' && path[1] == '/') return true;
#else
    // Unix-style absolute path
    if (path[0] == '/') return true;
#endif

    return false;
}

void ppdb_base_get_dirname(char* path) {
    if (!path) return;

    char* last_slash = strrchr(path, '/');
    if (last_slash) {
        *last_slash = '\0';
    } else {
        path[0] = '.';
        path[1] = '\0';
    }
}

void ppdb_base_get_basename(const char* path, char* basename, size_t size) {
    if (!path || !basename || size == 0) return;

    const char* last_slash = strrchr(path, '/');
    const char* name = last_slash ? last_slash + 1 : path;
    
    strncpy(basename, name, size - 1);
    basename[size - 1] = '\0';
}

// Random number generation
static uint64_t ppdb_base_rand_seed = 0;

void ppdb_base_rand_init(uint64_t seed) {
    ppdb_base_rand_seed = seed ? seed : (uint64_t)ppdb_base_get_time_us();
}

uint32_t ppdb_base_rand(void) {
    // XorShift algorithm
    ppdb_base_rand_seed ^= ppdb_base_rand_seed << 13;
    ppdb_base_rand_seed ^= ppdb_base_rand_seed >> 7;
    ppdb_base_rand_seed ^= ppdb_base_rand_seed << 17;
    return (uint32_t)ppdb_base_rand_seed;
}

uint32_t ppdb_base_rand_range(uint32_t min, uint32_t max) {
    if (min >= max) return min;
    return min + (ppdb_base_rand() % (max - min + 1));
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
        return (uint64_t)si.totalram * si.mem_unit;
    }
    return 0;
#endif
}