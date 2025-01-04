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

// 创建同步对象
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config) {
    if (!sync || !config) return PPDB_ERR_NULL_POINTER;
    
    *sync = PPDB_ALIGNED_ALLOC(sizeof(ppdb_sync_t));
    if (!*sync) return PPDB_ERR_OUT_OF_MEMORY;
    memset(*sync, 0, sizeof(ppdb_sync_t));
    
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
    
    // 复制配置
    sync->config = *config;
    
    // 初始化统计信息
    ppdb_sync_counter_init(&sync->stats.read_locks, 0);
    ppdb_sync_counter_init(&sync->stats.write_locks, 0);
    ppdb_sync_counter_init(&sync->stats.read_timeouts, 0);
    ppdb_sync_counter_init(&sync->stats.write_timeouts, 0);
    ppdb_sync_counter_init(&sync->stats.retries, 0);
    
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
    
    // 销毁统计信息
    ppdb_sync_counter_destroy(&sync->stats.read_locks);
    ppdb_sync_counter_destroy(&sync->stats.write_locks);
    ppdb_sync_counter_destroy(&sync->stats.read_timeouts);
    ppdb_sync_counter_destroy(&sync->stats.write_timeouts);
    ppdb_sync_counter_destroy(&sync->stats.retries);
    
    // 根据类型销毁锁
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
            
        case PPDB_SYNC_SPINLOCK: {
            uint32_t retries = 0;
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                if (++retries > sync->config.max_retries) {
                    ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
                    return PPDB_ERR_TIMEOUT;
                }
                if (sync->config.backoff_us > 0) {
                    usleep(sync->config.backoff_us);
                }
                ppdb_sync_counter_add(&sync->stats.retries, 1);
            }
            break;
        }
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_wrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    return PPDB_OK;
}

// 尝试加锁
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_trylock(&sync->mutex) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_trywrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
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
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    return PPDB_OK;
}

// 读锁
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_lock(&sync->mutex) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK: {
            uint32_t retries = 0;
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                if (++retries > sync->config.max_retries) {
                    ppdb_sync_counter_add(&sync->stats.read_timeouts, 1);
                    return PPDB_ERR_TIMEOUT;
                }
                if (sync->config.backoff_us > 0) {
                    usleep(sync->config.backoff_us);
                }
                ppdb_sync_counter_add(&sync->stats.retries, 1);
            }
            break;
        }
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_rdlock(&sync->rwlock) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.read_locks, 1);
    return PPDB_OK;
}

// 写锁
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_lock(&sync->mutex) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK: {
            uint32_t retries = 0;
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                if (++retries > sync->config.max_retries) {
                    ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
                    return PPDB_ERR_TIMEOUT;
                }
                if (sync->config.backoff_us > 0) {
                    usleep(sync->config.backoff_us);
                }
                ppdb_sync_counter_add(&sync->stats.retries, 1);
            }
            break;
        }
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_wrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    return PPDB_OK;
}

// 读解锁
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
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
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    return PPDB_OK;
}

// 写解锁
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
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
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    return PPDB_OK;
}

// 尝试获取读锁
ppdb_error_t ppdb_sync_try_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_trylock(&sync->mutex) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_tryrdlock(&sync->rwlock) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.read_locks, 1);
    return PPDB_OK;
}

// 尝试获取写锁
ppdb_error_t ppdb_sync_try_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_trylock(&sync->mutex) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_trywrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
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
    counter->lock = NULL;  // 默认不使用锁
    
    #ifdef PPDB_ENABLE_METRICS
    atomic_init(&counter->add_count, 0);
    atomic_init(&counter->sub_count, 0);
    counter->local_add_count = 0;
    counter->local_sub_count = 0;
    #endif
    
    return PPDB_OK;
}

void ppdb_sync_counter_destroy(ppdb_sync_counter_t* counter) {
    if (!counter) return;
    
    if (counter->lock) {
        ppdb_sync_destroy(counter->lock);
        counter->lock = NULL;
    }
}

size_t ppdb_sync_counter_add(ppdb_sync_counter_t* counter, size_t delta) {
    if (!counter) return 0;
    
    size_t old_value;
    if (counter->lock) {
        ppdb_sync_write_lock(counter->lock);
        old_value = atomic_load(&counter->value);
        atomic_store(&counter->value, old_value + delta);
        ppdb_sync_write_unlock(counter->lock);
    } else {
        old_value = atomic_fetch_add(&counter->value, delta);
    }
    
    #ifdef PPDB_ENABLE_METRICS
    atomic_fetch_add(&counter->add_count, 1);
    counter->local_add_count++;
    #endif
    
    return old_value;
}

size_t ppdb_sync_counter_sub(ppdb_sync_counter_t* counter, size_t delta) {
    if (!counter) return 0;
    
    size_t old_value;
    if (counter->lock) {
        ppdb_sync_write_lock(counter->lock);
        old_value = atomic_load(&counter->value);
        atomic_store(&counter->value, old_value - delta);
        ppdb_sync_write_unlock(counter->lock);
    } else {
        old_value = atomic_fetch_sub(&counter->value, delta);
    }
    
    #ifdef PPDB_ENABLE_METRICS
    atomic_fetch_add(&counter->sub_count, 1);
    counter->local_sub_count++;
    #endif
    
    return old_value;
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
