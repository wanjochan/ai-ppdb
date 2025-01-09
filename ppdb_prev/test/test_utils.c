#include "test_utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

void test_init_logger(void) {
    ppdb_logger_init(PPDB_LOG_DEBUG);
}

void test_cleanup_logger(void) {
    ppdb_logger_cleanup();
}

char* test_create_temp_dir(void) {
    char template[] = "ppdb_test_XXXXXX";
    char* dir_path = _mktemp(template);
    if (!dir_path) {
        return NULL;
    }
    if (_mkdir(dir_path) != 0) {
        return NULL;
    }
    return strdup(dir_path);
}

void test_remove_dir(const char* dir_path) {
    char search_path[PATH_MAX];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            test_remove_dir(path);
        } else {
            DeleteFile(path);
        }
    } while (FindNextFile(find_handle, &find_data));

    FindClose(find_handle);
    RemoveDirectory(dir_path);
}

void test_generate_random_data(void* buffer, size_t size) {
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    unsigned char* p = buffer;
    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)(rand() % 256);
    }
}

void test_generate_random_string(char* buffer, size_t size) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    if (size > 0) {
        size_t i;
        for (i = 0; i < size - 1; i++) {
            buffer[i] = charset[rand() % (sizeof(charset) - 1)];
        }
        buffer[i] = '\0';
    }
}

bool test_compare_memory(const void* a, const void* b, size_t size) {
    return memcmp(a, b, size) == 0;
}

bool test_file_exists(const char* path) {
    DWORD attrs = GetFileAttributes(path);
    return attrs != INVALID_FILE_ATTRIBUTES;
}

size_t test_file_size(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesEx(path, GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    LARGE_INTEGER size;
    size.HighPart = attrs.nFileSizeHigh;
    size.LowPart = attrs.nFileSizeLow;
    return (size_t)size.QuadPart;
}

bool test_is_directory(const char* path) {
    DWORD attrs = GetFileAttributes(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

uint64_t test_get_current_time_us(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)((count.QuadPart * 1000000) / freq.QuadPart);
}

void test_sleep_us(uint64_t microseconds) {
    Sleep((DWORD)((microseconds + 999) / 1000));
} 