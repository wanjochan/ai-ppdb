#include "fs.h"
#include <cosmopolitan.h>

ppdb_error_t ppdb_ensure_directory(const char* path) {
    if (path == NULL) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 检查目录是否已存在
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return PPDB_OK;
        }
        return PPDB_ERR_EXISTS;
    }

    // 创建目录
    if (mkdir(path, 0755) != 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_remove_directory(const char* path) {
    if (path == NULL) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 检查目录是否存在
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            return PPDB_OK;  // 目录不存在，视为成功
        }
        return PPDB_ERR_IO;
    }

    if (!S_ISDIR(st.st_mode)) {
        return PPDB_ERR_NOT_SUPPORTED;
    }

    // 打开目录
    DIR* dir = opendir(path);
    if (dir == NULL) {
        return PPDB_ERR_IO;
    }

    struct dirent* entry;
    char full_path[PATH_MAX];
    size_t path_len = strlen(path);
    strncpy(full_path, path, sizeof(full_path) - 1);
    full_path[sizeof(full_path) - 1] = '\0';

    if (full_path[path_len - 1] != '/') {
        if (path_len + 1 >= sizeof(full_path)) {
            closedir(dir);
            return PPDB_ERR_INVALID_ARG;
        }
        full_path[path_len] = '/';
        full_path[path_len + 1] = '\0';
        path_len++;
    }

    // 遍历目录内容
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (path_len + strlen(entry->d_name) >= sizeof(full_path)) {
            closedir(dir);
            return PPDB_ERR_INVALID_ARG;
        }

        strcpy(full_path + path_len, entry->d_name);

        if (stat(full_path, &st) != 0) {
            closedir(dir);
            return PPDB_ERR_IO;
        }

        if (S_ISDIR(st.st_mode)) {
            // 递归删除子目录
            ppdb_error_t err = ppdb_remove_directory(full_path);
            if (err != PPDB_OK) {
                closedir(dir);
                return err;
            }
        } else {
            // 删除文件
            if (unlink(full_path) != 0) {
                closedir(dir);
                return PPDB_ERR_IO;
            }
        }
    }

    closedir(dir);

    // 删除空目录
    if (rmdir(path) != 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
} 