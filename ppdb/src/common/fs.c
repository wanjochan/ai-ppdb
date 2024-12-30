#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_logger.h"

// 内部头文件
#include "kvstore/internal/kvstore_fs.h"

bool ppdb_fs_dir_exists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return false;
}

int ppdb_fs_mkdir(const char* path) {
#ifdef _WIN32
    return mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

ppdb_error_t ppdb_ensure_directory(const char* path) {
    if (!path) return PPDB_ERR_NULL_POINTER;

    // Check if directory exists
    if (ppdb_fs_dir_exists(path)) {
        return PPDB_OK;
    }

    // Create directory
    if (ppdb_fs_mkdir(path) != 0) {
        ppdb_log_error("Failed to create directory: %s", path);
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_remove_directory(const char* path) {
    if (!path) return PPDB_ERR_NULL_POINTER;

    DIR* dir = opendir(path);
    if (!dir) {
        if (errno == ENOENT) return PPDB_OK;  // Directory does not exist, consider success
        ppdb_log_error("Failed to open directory: %s", path);
        return PPDB_ERR_IO;
    }

    struct dirent* entry;
    char full_path[MAXPATHLEN];
    ppdb_error_t err = PPDB_OK;
    size_t path_len = strlen(path);

    // Safety check: ensure base path will not overflow
    if (path_len >= MAXPATHLEN - 2) {  // Reserve "/" and null terminator
        ppdb_log_error("Base path too long: %s", path);
        closedir(dir);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        size_t name_len = strlen(entry->d_name);
        // Check full path length
        if (path_len + name_len + 2 > MAXPATHLEN) {  // +2 for "/" and null terminator
            ppdb_log_error("Path would be too long: %s/%s", path, entry->d_name);
            err = PPDB_ERR_PATH_TOO_LONG;
            break;
        }

        ssize_t written_len = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (written_len < 0 || (size_t)written_len >= sizeof(full_path)) {
            ppdb_log_error("Failed to construct path: %s/%s", path, entry->d_name);
            err = PPDB_ERR_PATH_TOO_LONG;
            break;
        }

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recursively remove subdirectory
                err = ppdb_remove_directory(full_path);
                if (err != PPDB_OK) {
                    ppdb_log_error("Failed to remove directory: %s", full_path);
                    break;
                }
            } else {
                // Remove file before waiting for a short period
                usleep(50000);  // 50ms
                if (remove(full_path) != 0) {
                    ppdb_log_error("Failed to remove file: %s (errno: %d)", full_path, errno);
                    err = PPDB_ERR_IO;
                    break;
                }
            }
        }
    }

    closedir(dir);

    // Only attempt to remove the directory itself after all sub-items have been successfully removed
    if (err == PPDB_OK) {
        // Wait for a sufficient amount of time to ensure all file handles are closed
        usleep(100000);  // 100ms

        // Attempt to remove the directory multiple times
        int retries = 3;
        while (retries > 0) {
            if (rmdir(path) == 0) {
                break;
            }
            ppdb_log_warn("Failed to remove directory: %s (errno: %d), retrying...", path, errno);
            usleep(100000);  // Wait 100ms before each retry
            retries--;
        }

        if (retries == 0) {
            ppdb_log_error("Failed to remove directory after retries: %s (errno: %d)", path, errno);
            err = PPDB_ERR_IO;
        }
    }

    return err;
}

bool ppdb_fs_file_exists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISREG(st.st_mode);
    }
    return false;
}

ppdb_error_t ppdb_get_file_size(const char* path, size_t* size) {
    if (!path || !size) return PPDB_ERR_NULL_POINTER;

    struct stat st;
    if (stat(path, &st) != 0) {
        ppdb_log_error("Failed to get file size: %s (errno: %d)", path, errno);
        return PPDB_ERR_IO;
    }

    *size = (size_t)st.st_size;
    return PPDB_OK;
}

ppdb_error_t ppdb_read_file(const char* path, void* buf, size_t size) {
    if (!path || !buf) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_OK;

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        ppdb_log_error("Failed to open file for reading: %s (errno: %d)", path, errno);
        return PPDB_ERR_IO;
    }

    size_t bytes_read = fread(buf, 1, size, fp);
    if (bytes_read != size) {
        ppdb_log_error("Failed to read file: %s (expected: %zu, actual: %zu)", 
                      path, size, bytes_read);
        fclose(fp);
        return PPDB_ERR_IO;
    }

    fclose(fp);
    return PPDB_OK;
}

ppdb_error_t ppdb_write_file(const char* path, const void* buf, size_t size) {
    if (!path || !buf) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_OK;

    FILE* fp = fopen(path, "wb");
    if (!fp) {
        ppdb_log_error("Failed to open file for writing: %s (errno: %d)", path, errno);
        return PPDB_ERR_IO;
    }

    size_t bytes_written = fwrite(buf, 1, size, fp);
    if (bytes_written != size) {
        ppdb_log_error("Failed to write file: %s (expected: %zu, actual: %zu)", 
                      path, size, bytes_written);
        fclose(fp);
        return PPDB_ERR_IO;
    }

    fclose(fp);
    return PPDB_OK;
}

ppdb_error_t ppdb_append_file(const char* path, const void* buf, size_t size) {
    if (!path || !buf) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_OK;

    FILE* fp = fopen(path, "ab");
    if (!fp) {
        ppdb_log_error("Failed to open file for appending: %s (errno: %d)", path, errno);
        return PPDB_ERR_IO;
    }

    size_t bytes_written = fwrite(buf, 1, size, fp);
    if (bytes_written != size) {
        ppdb_log_error("Failed to append to file: %s (expected: %zu, actual: %zu)", 
                      path, size, bytes_written);
        fclose(fp);
        return PPDB_ERR_IO;
    }

    fclose(fp);
    return PPDB_OK;
}

ppdb_error_t ppdb_truncate_file(const char* path, size_t size) {
    if (!path) return PPDB_ERR_NULL_POINTER;

#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 
                             FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        ppdb_log_error("Failed to open file for truncating: %s", path);
        return PPDB_ERR_IO;
    }

    LARGE_INTEGER li;
    li.QuadPart = size;
    if (!SetFilePointerEx(hFile, li, NULL, FILE_BEGIN) || !SetEndOfFile(hFile)) {
        ppdb_log_error("Failed to truncate file: %s", path);
        CloseHandle(hFile);
        return PPDB_ERR_IO;
    }

    CloseHandle(hFile);
#else
    if (truncate(path, size) != 0) {
        ppdb_log_error("Failed to truncate file: %s (errno: %d)", path, errno);
        return PPDB_ERR_IO;
    }
#endif

    return PPDB_OK;
}
