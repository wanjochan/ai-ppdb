#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 错误处理实现
//-----------------------------------------------------------------------------

static const char* error_messages[] = {
    "成功",                     // PPDB_OK
    "空指针",                   // PPDB_ERR_NULL_POINTER
    "内存不足",                 // PPDB_ERR_OUT_OF_MEMORY
    "未找到",                   // PPDB_ERR_NOT_FOUND
    "已存在",                   // PPDB_ERR_ALREADY_EXISTS
    "无效类型",                 // PPDB_ERR_INVALID_TYPE
    "无效状态",                 // PPDB_ERR_INVALID_STATE
    "内部错误",                 // PPDB_ERR_INTERNAL
    "不支持",                   // PPDB_ERR_NOT_SUPPORTED
    "存储已满",                 // PPDB_ERR_FULL
    "存储为空",                 // PPDB_ERR_EMPTY
    "数据损坏",                 // PPDB_ERR_CORRUPTED
    "IO错误",                   // PPDB_ERR_IO
    "资源忙",                   // PPDB_ERR_BUSY
    "超时",                     // PPDB_ERR_TIMEOUT
};

const char* ppdb_strerror(ppdb_error_t err) {
    if (err == 0) return error_messages[0];
    if (err < -33 || err > 0) return "未知错误";
    return error_messages[-err];
}

ppdb_error_t ppdb_system_error(int err) {
    if (err == ENOMEM) return PPDB_ERR_OUT_OF_MEMORY;
    if (err == EEXIST) return PPDB_ERR_ALREADY_EXISTS;
    if (err == ENOENT) return PPDB_ERR_NOT_FOUND;
    if (err == EBUSY) return PPDB_ERR_BUSY;
    if (err == EIO) return PPDB_ERR_IO;
    if (err == EAGAIN) return PPDB_ERR_BUSY;
    return PPDB_ERR_INTERNAL;
}

//-----------------------------------------------------------------------------
// 日志系统实现
//-----------------------------------------------------------------------------

static ppdb_log_config_t log_config = {0};
static ppdb_sync_t* log_sync = NULL;
static FILE* log_file = NULL;
static ppdb_log_level_t min_level = PPDB_LOG_INFO;

static const char* level_strings[] = {
    [PPDB_LOG_DEBUG] = "DEBUG",
    [PPDB_LOG_INFO] = "INFO",
    [PPDB_LOG_WARN] = "WARN",
    [PPDB_LOG_ERROR] = "ERROR",
    [PPDB_LOG_FATAL] = "FATAL"
};

void ppdb_log_init(const ppdb_log_config_t* config) {
    if (!config) return;

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

void ppdb_debug(const char* fmt, ...) {
    if (!log_config.enabled || PPDB_LOG_DEBUG < min_level) {
        return;
    }

    ppdb_sync_lock(log_sync);

    va_list args;
    va_start(args, fmt);

    if (log_file) {
        fprintf(log_file, "[DEBUG] ");
        vfprintf(log_file, fmt, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    if (log_config.outputs & 1) {
        fprintf(stdout, "[DEBUG] ");
        vfprintf(stdout, fmt, args);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    va_end(args);
    ppdb_sync_unlock(log_sync);
}

//-----------------------------------------------------------------------------
// 文件系统操作实现
//-----------------------------------------------------------------------------

// 文件检查函数
bool ppdb_fs_exists(const char* path) {
    if (!path) {
        ppdb_log(PPDB_LOG_ERROR, "Null path provided to ppdb_fs_exists");
        return false;
    }
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
        return ppdb_system_error(errno);
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_init(const char* path) {
    ppdb_debug("Initializing filesystem at: %s", path);
    if (!path) {
        ppdb_log(PPDB_LOG_ERROR, "Null path provided to ppdb_fs_init");
        return PPDB_ERR_NULL_POINTER;
    }

    // Add state validation
    if (strlen(path) >= 1024) {
        ppdb_log(PPDB_LOG_ERROR, "Path too long");
        return PPDB_ERR_INVALID_STATE;
    }

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
            return ppdb_system_error(errno);
        }
    }

    // 删除主目录
    if (rmdir(path) != 0) {
        return ppdb_system_error(errno);
    }

    return PPDB_OK;
}

// 文件操作函数
ppdb_error_t ppdb_fs_write(const char* path, const void* data, size_t size) {
    ppdb_debug("Writing %zu bytes to %s", size, path);
    
    // Enhanced parameter validation
    if (!path || !data) {
        ppdb_log(PPDB_LOG_ERROR, "Null pointer in ppdb_fs_write");
        return PPDB_ERR_NULL_POINTER;
    }

    if (size == 0) {
        ppdb_log(PPDB_LOG_WARN, "Zero-length write requested");
        return PPDB_OK;
    }

    FILE* fp = fopen(path, "wb");
    if (!fp) {
        ppdb_log(PPDB_LOG_ERROR, "Failed to open file for writing: %s", path);
        return ppdb_system_error(errno);
    }

    // Add memory operation tracking
    ppdb_debug("Starting write operation of %zu bytes", size);
    size_t written = fwrite(data, 1, size, fp);
    if (written != size) {
        ppdb_log(PPDB_LOG_ERROR, "Partial write: %zu of %zu bytes", written, size);
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
    if (!fp) return ppdb_system_error(errno);

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
    if (!fp) return ppdb_system_error(errno);

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
// 同步原语实现
//-----------------------------------------------------------------------------

#include "base_sync.inc.c"

//-----------------------------------------------------------------------------
// 内存分配实现
//-----------------------------------------------------------------------------

void* aligned_alloc(size_t alignment, size_t size) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
}

void aligned_free(void* ptr) {
    free(ptr);
}

//-----------------------------------------------------------------------------
// 其他辅助函数
//-----------------------------------------------------------------------------

uint64_t ppdb_random(void) {
    static _Atomic uint64_t counter = 0;
    uint64_t value = atomic_fetch_add(&counter, 1);
    value = value * 2862933555777941757ULL + 3037000493ULL;
    value = (value >> 32) | (value << 32);
    return value;
}

uint32_t random_level(void) {
    uint32_t level = 1;
    while (level < PPDB_MAX_HEIGHT && (ppdb_random() % 100) < PPDB_LEVEL_PROBABILITY) {
        level++;
    }
    return level;
}

ppdb_error_t validate_and_setup_config(ppdb_config_t* config) {
    if (!config) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    if (config->shard_count == 0 || config->shard_count > PPDB_MAX_SHARDS) {
        return PPDB_ERR_INVALID_CONFIG;
    }

    return PPDB_OK;
}

ppdb_error_t init_metrics(ppdb_metrics_t* metrics) {
    if (!metrics) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }
    memset(metrics, 0, sizeof(ppdb_metrics_t));
    ppdb_sync_counter_init(&metrics->total_nodes, 0);
    ppdb_sync_counter_init(&metrics->total_keys, 0);
    ppdb_sync_counter_init(&metrics->total_bytes, 0);
    ppdb_sync_counter_init(&metrics->total_gets, 0);
    ppdb_sync_counter_init(&metrics->total_puts, 0);
    ppdb_sync_counter_init(&metrics->total_removes, 0);
    return PPDB_OK;
}
