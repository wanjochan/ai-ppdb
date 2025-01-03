#include "ppdb/ppdb.h"
#include "ppdb/ppdb_internal.h"
#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 错误处理实现
//-----------------------------------------------------------------------------

static const char* error_messages[] = {
    [PPDB_OK] = "成功",
    [PPDB_ERR_NULL_POINTER] = "空指针",
    [PPDB_ERR_OUT_OF_MEMORY] = "内存不足",
    [PPDB_ERR_NOT_FOUND] = "未找到",
    [PPDB_ERR_ALREADY_EXISTS] = "已存在",
    [PPDB_ERR_INVALID_STATE] = "无效状态",
    [PPDB_ERR_BUSY] = "资源忙",
    [PPDB_ERR_TIMEOUT] = "超时",
    [PPDB_ERR_TOO_MANY_READERS] = "读者过多",
    [PPDB_ERR_NOT_SUPPORTED] = "不支持",
    [PPDB_ERR_INTERNAL] = "内部错误",
    [PPDB_ERR_UNLOCK_FAILED] = "解锁失败",
    [PPDB_ERR_RETRY] = "需要重试",
    [PPDB_ERR_SYNC_RETRY_FAILED] = "重试失败"
};

const char* ppdb_strerror(ppdb_error_t err) {
    if (err < 0 || err >= PPDB_ERR_SYNC_RETRY_FAILED + 1) {
        return "未知错误";
    }
    return error_messages[err];
}

ppdb_error_t ppdb_system_error(int err) {
    switch (err) {
        case ENOMEM:
            return PPDB_ERR_OUT_OF_MEMORY;
        case EEXIST:
            return PPDB_ERR_ALREADY_EXISTS;
        case ENOENT:
            return PPDB_ERR_NOT_FOUND;
        case EBUSY:
            return PPDB_ERR_BUSY;
        case EIO:
            return PPDB_ERR_INTERNAL;
        case EAGAIN:
            return PPDB_ERR_RETRY;
        default:
            return PPDB_ERR_INTERNAL;
    }
}

//-----------------------------------------------------------------------------
// 文件系统操作实现
//-----------------------------------------------------------------------------

// 文件检查函数
bool ppdb_fs_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool ppdb_fs_is_file(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool ppdb_fs_is_dir(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// 目录操作函数
static ppdb_error_t ensure_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return PPDB_OK;
        }
        return PPDB_ERR_ALREADY_EXISTS;
    }
    
    if (mkdir(path, 0755) != 0) {
        return ppdb_system_error();
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_init(const char* path) {
    if (!path) return PPDB_ERR_NULL_POINTER;

    // 创建主目录
    ppdb_error_t err = ensure_directory(path);
    if (err != PPDB_OK) return err;

    // 创建子目录
    char subdir[1024];
    const char* subdirs[] = {"data", "wal", "tmp"};
    for (size_t i = 0; i < sizeof(subdirs)/sizeof(subdirs[0]); i++) {
        snprintf(subdir, sizeof(subdir), "%s/%s", path, subdirs[i]);
        err = ensure_directory(subdir);
        if (err != PPDB_OK) return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_fs_cleanup(const char* path) {
    if (!path) return PPDB_ERR_NULL_POINTER;

    // 删除子目录
    char subdir[1024];
    const char* subdirs[] = {"data", "wal", "tmp"};
    for (size_t i = 0; i < sizeof(subdirs)/sizeof(subdirs[0]); i++) {
        snprintf(subdir, sizeof(subdir), "%s/%s", path, subdirs[i]);
        if (rmdir(subdir) != 0) {
            return ppdb_system_error();
        }
    }

    // 删除主目录
    if (rmdir(path) != 0) {
        return ppdb_system_error();
    }

    return PPDB_OK;
}

// 文件操作函数
ppdb_error_t ppdb_fs_write(const char* path, const void* data, size_t size) {
    if (!path || !data) return PPDB_ERR_NULL_POINTER;

    FILE* fp = fopen(path, "wb");
    if (!fp) return ppdb_system_error();

    size_t written = fwrite(data, 1, size, fp);
    if (written != size) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    if (fsync(fileno(fp)) != 0) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    fclose(fp);
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_read(const char* path, void* data, size_t size, size_t* bytes_read) {
    if (!path || !data || !bytes_read) return PPDB_ERR_NULL_POINTER;

    FILE* fp = fopen(path, "rb");
    if (!fp) return ppdb_system_error();

    *bytes_read = fread(data, 1, size, fp);
    if (*bytes_read == 0 && ferror(fp)) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    fclose(fp);
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_append(const char* path, const void* data, size_t size) {
    if (!path || !data) return PPDB_ERR_NULL_POINTER;

    FILE* fp = fopen(path, "ab");
    if (!fp) return ppdb_system_error();

    size_t written = fwrite(data, 1, size, fp);
    if (written != size) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    if (fsync(fileno(fp)) != 0) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    fclose(fp);
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// 日志系统实现
//-----------------------------------------------------------------------------

typedef enum {
    PPDB_LOG_DEBUG = 0,
    PPDB_LOG_INFO,
    PPDB_LOG_WARN,
    PPDB_LOG_ERROR,
    PPDB_LOG_FATAL
} ppdb_log_level_t;

typedef struct {
    bool enabled;
    ppdb_log_level_t level;
    const char* log_file;
    int outputs;
} ppdb_log_config_t;

static const char* level_strings[] = {
    [PPDB_LOG_DEBUG] = "DEBUG",
    [PPDB_LOG_INFO] = "INFO",
    [PPDB_LOG_WARN] = "WARN",
    [PPDB_LOG_ERROR] = "ERROR",
    [PPDB_LOG_FATAL] = "FATAL"
};

static ppdb_log_config_t log_config = {0};
static ppdb_sync_t* log_sync = NULL;
static FILE* log_file = NULL;
static ppdb_log_level_t min_level = PPDB_LOG_INFO;

void ppdb_log_init(const ppdb_log_config_t* config) {
    if (!config) {
        return;
    }

    // 创建同步对象
    if (!log_sync) {
        ppdb_sync_config_t sync_config = {
            .type = PPDB_SYNC_MUTEX,
            .use_lockfree = false,
            .enable_ref_count = false,
            .max_readers = 1,
            .backoff_us = 1,
            .max_retries = 100
        };
        ppdb_sync_create(&log_sync, &sync_config);
    }

    ppdb_sync_lock(log_sync);
    
    log_config = *config;
    min_level = config->level;

    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }

    if (config->log_file) {
        log_file = fopen(config->log_file, "a");
    }

    ppdb_sync_unlock(log_sync);
}

void ppdb_log_cleanup(void) {
    if (log_sync) {
        ppdb_sync_lock(log_sync);
        if (log_file) {
            fclose(log_file);
            log_file = NULL;
        }
        ppdb_sync_unlock(log_sync);
        ppdb_sync_destroy(log_sync);
        free(log_sync);
        log_sync = NULL;
    }
}

void ppdb_log(ppdb_log_level_t level, const char* fmt, ...) {
    if (!log_config.enabled || level < min_level) {
        return;
    }

    ppdb_sync_lock(log_sync);

    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    va_list args;
    va_start(args, fmt);

    if (log_file) {
        fprintf(log_file, "[%s] [%s] ", time_str, level_strings[level]);
        vfprintf(log_file, fmt, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    if (log_config.outputs & 1) {
        fprintf(stdout, "[%s] [%s] ", time_str, level_strings[level]);
        vfprintf(stdout, fmt, args);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    va_end(args);
    ppdb_sync_unlock(log_sync);
}

// ... existing code ...
