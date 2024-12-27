#include <cosmopolitan.h>
#include "ppdb/error.h"
#include "ppdb/defs.h"
#include "../common/logger.h"

// 确保目录存在
ppdb_error_t ppdb_ensure_directory(const char* path) {
    if (!path) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 检查目录是否已存在
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return PPDB_OK;  // 目录已存在
        }
        return PPDB_ERR_EXISTS;  // 路径存在但不是目录
    }

    // 创建目录
    char tmp[MAX_PATH_LENGTH];
    char *p;

    // 复制路径
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    // 递归创建目录
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                ppdb_log_error("Failed to create directory: %s, error: %s", tmp, strerror(errno));
                return PPDB_ERR_IO;
            }
            *p = '/';  // 统一使用正斜杠
        }
    }

    // 创建最后一级目录
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        ppdb_log_error("Failed to create directory: %s, error: %s", tmp, strerror(errno));
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
} 