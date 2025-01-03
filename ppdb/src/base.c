#include "ppdb/ppdb.h"
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
        return ppdb_system_error(errno);
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
    if (!path || !data) return PPDB_ERR_NULL_POINTER;

    FILE* fp = fopen(path, "wb");
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

//-----------------------------------------------------------------------------
// 同步原语实现
//-----------------------------------------------------------------------------

// 创建同步对象
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config) {
    if (!sync || !config) return PPDB_ERR_NULL_POINTER;
    
    *sync = PPDB_ALIGNED_ALLOC(sizeof(ppdb_sync_t));
    if (!*sync) return PPDB_ERR_OUT_OF_MEMORY;
    
    ppdb_error_t err = ppdb_sync_init(*sync, config);
    if (err != PPDB_OK) {
        PPDB_ALIGNED_FREE(*sync);
        *sync = NULL;
    }
    return err;
}

// 初始化同步对象
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_config_t* config) {
    if (!sync || !config) return PPDB_ERR_NULL_POINTER;
    
    memset(sync, 0, sizeof(ppdb_sync_t));
    sync->config = *config;
    
    // 初始化统计信息
    memset(&sync->stats, 0, sizeof(ppdb_sync_stats_t));
    
    // 根据类型初始化锁
    switch (config->type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_init(&sync->mutex, NULL) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_init(&sync->rwlock, NULL) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    return PPDB_OK;
}

// 销毁同步对象
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            pthread_mutex_destroy(&sync->mutex);
            break;
            
        case PPDB_SYNC_SPINLOCK:
            // 自旋锁不需要销毁
            break;
            
        case PPDB_SYNC_RWLOCK:
            pthread_rwlock_destroy(&sync->rwlock);
            break;
    }
    
    return PPDB_OK;
}

// 加锁
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_lock(&sync->mutex) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                // 自旋等待
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_wrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
    }
    
    return PPDB_OK;
}

// 尝试加锁
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_trylock(&sync->mutex) != 0) {
                ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_trywrlock(&sync->rwlock) != 0) {
                ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
                return PPDB_ERR_BUSY;
            }
            break;
    }
    
    ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    return PPDB_OK;
}

// 解锁
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_unlock(&sync->mutex) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            break;
    }
    
    return PPDB_OK;
}

// 读锁
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_lock(sync);
    }
    
    if (sync->config.use_lockfree) {
        // 无锁模式下使用自旋等待
        uint32_t retries = 0;
        while (true) {
            if (pthread_rwlock_tryrdlock(&sync->rwlock) == 0) {
                ppdb_sync_counter_add(&sync->stats.read_locks, 1);
                return PPDB_OK;
            }
            
            if (++retries >= sync->config.max_retries) {
                ppdb_sync_counter_add(&sync->stats.read_timeouts, 1);
                return PPDB_ERR_TIMEOUT;
            }
            
            if (sync->config.backoff_us > 0) {
                usleep(sync->config.backoff_us);
            }
            
            ppdb_sync_counter_add(&sync->stats.retries, 1);
        }
    } else {
        // 有锁模式下直接阻塞等待
        if (pthread_rwlock_rdlock(&sync->rwlock) != 0) {
            return PPDB_ERR_LOCK_FAILED;
        }
        ppdb_sync_counter_add(&sync->stats.read_locks, 1);
    }
    
    return PPDB_OK;
}

// 写锁
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_lock(sync);
    }
    
    if (sync->config.use_lockfree) {
        // 无锁模式下使用自旋等待
        uint32_t retries = 0;
        while (true) {
            if (pthread_rwlock_trywrlock(&sync->rwlock) == 0) {
                ppdb_sync_counter_add(&sync->stats.write_locks, 1);
                return PPDB_OK;
            }
            
            if (++retries >= sync->config.max_retries) {
                ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
                return PPDB_ERR_TIMEOUT;
            }
            
            if (sync->config.backoff_us > 0) {
                usleep(sync->config.backoff_us);
            }
            
            ppdb_sync_counter_add(&sync->stats.retries, 1);
        }
    } else {
        // 有锁模式下直接阻塞等待
        if (pthread_rwlock_wrlock(&sync->rwlock) != 0) {
            return PPDB_ERR_LOCK_FAILED;
        }
        ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    }
    
    return PPDB_OK;
}

// 读解锁
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_unlock(sync);
    }
    
    if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
        return PPDB_ERR_UNLOCK_FAILED;
    }
    
    return PPDB_OK;
}

// 写解锁
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_unlock(sync);
    }
    
    if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
        return PPDB_ERR_UNLOCK_FAILED;
    }
    
    return PPDB_OK;
}

// 尝试获取读锁
ppdb_error_t ppdb_sync_try_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_try_lock(sync);
    }
    
    if (pthread_rwlock_tryrdlock(&sync->rwlock) != 0) {
        ppdb_sync_counter_add(&sync->stats.read_timeouts, 1);
        return PPDB_ERR_BUSY;
    }
    
    ppdb_sync_counter_add(&sync->stats.read_locks, 1);
    return PPDB_OK;
}

// 尝试获取写锁
ppdb_error_t ppdb_sync_try_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_try_lock(sync);
    }
    
    if (pthread_rwlock_trywrlock(&sync->rwlock) != 0) {
        ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
        return PPDB_ERR_BUSY;
    }
    
    ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    return PPDB_OK;
}

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
// 计数器操作实现
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_sync_counter_init(ppdb_sync_counter_t* counter, size_t initial_value) {
    if (!counter) return PPDB_ERR_NULL_POINTER;
    atomic_init(&counter->value, initial_value);
    counter->lock = NULL;
    #ifdef PPDB_ENABLE_METRICS
    atomic_init(&counter->add_count, 0);
    atomic_init(&counter->sub_count, 0);
    #endif
    return PPDB_OK;
}

void ppdb_sync_counter_destroy(ppdb_sync_counter_t* counter) {
    if (!counter) return;
    if (counter->lock) {
        ppdb_sync_destroy(counter->lock);
        aligned_free(counter->lock);
    }
}

size_t ppdb_sync_counter_add(ppdb_sync_counter_t* counter, size_t delta) {
    if (!counter) return 0;
    
    if (counter->lock) {
        ppdb_sync_lock(counter->lock);
    }
    
    size_t old_value = atomic_fetch_add(&counter->value, delta);
    
    #ifdef PPDB_ENABLE_METRICS
    atomic_fetch_add(&counter->add_count, 1);
    #endif
    
    if (counter->lock) {
        ppdb_sync_unlock(counter->lock);
    }
    
    return old_value + delta;
}

size_t ppdb_sync_counter_sub(ppdb_sync_counter_t* counter, size_t delta) {
    if (!counter) return 0;
    
    if (counter->lock) {
        ppdb_sync_lock(counter->lock);
    }
    
    size_t old_value = atomic_fetch_sub(&counter->value, delta);
    
    #ifdef PPDB_ENABLE_METRICS
    atomic_fetch_add(&counter->sub_count, 1);
    #endif
    
    if (counter->lock) {
        ppdb_sync_unlock(counter->lock);
    }
    
    return old_value - delta;
}

size_t ppdb_sync_counter_load(ppdb_sync_counter_t* counter) {
    if (!counter) return 0;
    return atomic_load(&counter->value);
}

void ppdb_sync_counter_store(ppdb_sync_counter_t* counter, size_t value) {
    if (!counter) return;
    atomic_store(&counter->value, value);
}

bool ppdb_sync_counter_cas(ppdb_sync_counter_t* counter, size_t expected, size_t desired) {
    if (!counter) return false;
    return atomic_compare_exchange_strong(&counter->value, &expected, desired);
}

// ... existing code ...
