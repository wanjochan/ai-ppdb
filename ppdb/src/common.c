#include <cosmopolitan.h>
#include "ppdb/ppdb.h"

//-----------------------------------------------------------------------------
// 错误处理
//-----------------------------------------------------------------------------

static const char* error_messages[] = {
    [PPDB_OK] = "成功",
    [PPDB_ERR_NULL_POINTER] = "空指针",
    [PPDB_ERR_OUT_OF_MEMORY] = "内存不足",
    [PPDB_ERR_NOT_FOUND] = "未找到",
    [PPDB_ERR_ALREADY_EXISTS] = "已存在",
    [PPDB_ERR_INVALID_TYPE] = "无效类型",
    [PPDB_ERR_LOCK_FAILED] = "加锁失败",
    [PPDB_ERR_FULL] = "存储已满",
    [PPDB_ERR_NOT_IMPLEMENTED] = "未实现",
    [PPDB_ERR_IO] = "IO错误",
    [PPDB_ERR_CORRUPTED] = "数据损坏",
    [PPDB_ERR_BUSY] = "资源忙",
    [PPDB_ERR_RETRY] = "需要重试"
};

const char* ppdb_error_string(ppdb_error_t err) {
    if (err >= 0 || -err >= (int)(sizeof(error_messages) / sizeof(error_messages[0]))) {
        return "未知错误";
    }
    return error_messages[-err];
}

ppdb_error_t ppdb_system_error(void) {
    switch (errno) {
        case ENOMEM:
            return PPDB_ERR_OUT_OF_MEMORY;
        case EEXIST:
            return PPDB_ERR_ALREADY_EXISTS;
        case ENOENT:
            return PPDB_ERR_NOT_FOUND;
        case EBUSY:
            return PPDB_ERR_BUSY;
        case EIO:
            return PPDB_ERR_IO;
        case EAGAIN:
            return PPDB_ERR_RETRY;
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
}

//-----------------------------------------------------------------------------
// 文件系统操作
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
// 日志系统
//-----------------------------------------------------------------------------

static FILE* log_file = NULL;
static ppdb_log_config_t log_config = {0};
static ppdb_rwlock_t log_lock = PPDB_RWLOCK_INIT;
static bool async_logging = false;
static ppdb_log_level_t min_level = PPDB_LOG_INFO;

void ppdb_log_init(const ppdb_log_config_t* config) {
    if (!config) return;

    ppdb_rwlock_wrlock(&log_lock);
    log_config = *config;
    async_logging = config->async_mode;
    min_level = config->level;

    if (log_config.enabled && (log_config.outputs & PPDB_LOG_FILE) && log_config.log_file) {
        log_file = fopen(log_config.log_file, "a");
    }
    ppdb_rwlock_unlock(&log_lock);
}

void ppdb_log_shutdown(void) {
    ppdb_rwlock_wrlock(&log_lock);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    memset(&log_config, 0, sizeof(log_config));
    ppdb_rwlock_unlock(&log_lock);
}

void ppdb_log_set_level(ppdb_log_level_t level) {
    ppdb_rwlock_wrlock(&log_lock);
    min_level = level;
    ppdb_rwlock_unlock(&log_lock);
}

void ppdb_log_enable_async(bool enable) {
    ppdb_rwlock_wrlock(&log_lock);
    async_logging = enable;
    ppdb_rwlock_unlock(&log_lock);
}

static void log_write(ppdb_log_level_t level, const char* fmt, va_list args) {
    if (!log_config.enabled || level < min_level) return;

    ppdb_rwlock_rdlock(&log_lock);

    char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    const char* level_str = "UNKNOWN";
    switch (level) {
        case PPDB_LOG_DEBUG: level_str = "DEBUG"; break;
        case PPDB_LOG_INFO:  level_str = "INFO"; break;
        case PPDB_LOG_WARN:  level_str = "WARN"; break;
        case PPDB_LOG_ERROR: level_str = "ERROR"; break;
        case PPDB_LOG_FATAL: level_str = "FATAL"; break;
    }

    char message[1024];
    vsnprintf(message, sizeof(message), fmt, args);

    if (log_config.outputs & PPDB_LOG_CONSOLE) {
        printf("[%s] [%s] %s\n", timestamp, level_str, message);
        fflush(stdout);
    }

    if ((log_config.outputs & PPDB_LOG_FILE) && log_file) {
        fprintf(log_file, "[%s] [%s] %s\n", timestamp, level_str, message);
        fflush(log_file);
    }

    ppdb_rwlock_unlock(&log_lock);
}

void ppdb_log(ppdb_log_level_t level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(level, fmt, args);
    va_end(args);
}

void ppdb_log_debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(PPDB_LOG_DEBUG, fmt, args);
    va_end(args);
}

void ppdb_log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(PPDB_LOG_INFO, fmt, args);
    va_end(args);
}

void ppdb_log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(PPDB_LOG_WARN, fmt, args);
    va_end(args);
}

void ppdb_log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(PPDB_LOG_ERROR, fmt, args);
    va_end(args);
}

void ppdb_log_fatal(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_write(PPDB_LOG_FATAL, fmt, args);
    va_end(args);
} 