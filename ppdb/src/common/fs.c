#include <cosmopolitan.h>
#include "fs.h"
#include "ppdb/defs.h"
#include "logger.h"

ppdb_error_t ppdb_ensure_directory(const char* path) {
    char tmp[MAX_PATH_LENGTH];
    char *p;
    size_t len;

    // 复制路径并移除末尾的斜杠
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    // 如果目录已存在，直接返回
    struct stat st;
    if (stat(tmp, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return PPDB_OK;
        }
        ppdb_log_error("Path exists but is not a directory: %s", tmp);
        return PPDB_ERR_IO;
    }

    // 递归创建目录
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                ppdb_log_error("Failed to create directory: %s, error: %s", tmp, strerror(errno));
                return PPDB_ERR_IO;
            }
            *p = '/';
        }
    }

    // 创建最后一级目录
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        ppdb_log_error("Failed to create directory: %s, error: %s", tmp, strerror(errno));
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
} 