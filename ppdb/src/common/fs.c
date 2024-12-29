#include <cosmopolitan.h>
#include "ppdb/fs.h"
#include "ppdb/error.h"
#include "ppdb/logger.h"

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

    // 检查目录是否已存在
    if (ppdb_fs_dir_exists(path)) {
        return PPDB_OK;
    }

    // 创建目录
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
        if (errno == ENOENT) return PPDB_OK;  // 目录不存在，视为成功
        ppdb_log_error("Failed to open directory: %s", path);
        return PPDB_ERR_IO;
    }

    struct dirent* entry;
    char full_path[MAX_PATH_LENGTH];
    ppdb_error_t err = PPDB_OK;
    size_t path_len = strlen(path);

    // 安全检查：确保基础路径不会溢出
    if (path_len >= MAX_PATH_LENGTH - 2) {  // 预留 "/" 和结束符
        ppdb_log_error("Base path too long: %s", path);
        closedir(dir);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        size_t name_len = strlen(entry->d_name);
        // 检查完整路径长度
        if (path_len + name_len + 2 > MAX_PATH_LENGTH) {  // +2 for "/" and null terminator
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
                // 递归删除子目录
                err = ppdb_remove_directory(full_path);
                if (err != PPDB_OK) {
                    ppdb_log_error("Failed to remove directory: %s", full_path);
                    break;
                }
            } else {
                // 删除文件前等待一段时间
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

    // 只有在所有子项都成功删除后，才尝试删除目录本身
    if (err == PPDB_OK) {
        // 等待足够长的时间，确保所有文件句柄都已关闭
        usleep(100000);  // 100ms

        // 多次尝试删除目录
        int retries = 3;
        while (retries > 0) {
            if (rmdir(path) == 0) {
                break;
            }
            ppdb_log_warn("Failed to remove directory: %s (errno: %d), retrying...", path, errno);
            usleep(100000);  // 每次重试前等待100ms
            retries--;
        }

        if (retries == 0) {
            ppdb_log_error("Failed to remove directory after retries: %s (errno: %d)", path, errno);
            err = PPDB_ERR_IO;
        }
    }

    return err;
} 