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
    char template[] = "/tmp/ppdb_test_XXXXXX";
    char* dir_path = mkdtemp(template);
    if (!dir_path) {
        return NULL;
    }
    return strdup(dir_path);
}

void test_remove_dir(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        
        struct stat st;
        if (stat(path, &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            test_remove_dir(path);
        } else {
            unlink(path);
        }
    }
    closedir(dir);
    rmdir(dir_path);
}

void test_generate_random_data(void* buffer, size_t size) {
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    unsigned char* buf = (unsigned char*)buffer;
    for (size_t i = 0; i < size; i++) {
        buf[i] = (unsigned char)(rand() % 256);
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
        size_t n = size - 1;
        for (size_t i = 0; i < n; i++) {
            int key = rand() % (sizeof(charset) - 1);
            buffer[i] = charset[key];
        }
        buffer[n] = '\0';
    }
}

bool test_compare_memory(const void* a, const void* b, size_t size) {
    return memcmp(a, b, size) == 0;
}

bool test_file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

size_t test_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return (size_t)st.st_size;
    }
    return 0;
}

bool test_is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

uint64_t test_get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

void test_sleep_us(uint64_t microseconds) {
    struct timespec ts;
    ts.tv_sec = microseconds / 1000000;
    ts.tv_nsec = (microseconds % 1000000) * 1000;
    nanosleep(&ts, NULL);
} 