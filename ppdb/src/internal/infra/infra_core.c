/*
 * infra.c - Infrastructure Layer Implementation
 */

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_error.h"

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

// 全局状态
static struct {
    bool initialized;
    infra_init_flags_t active_flags;
    infra_mutex_t mutex;

    // 日志系统状态
    struct {
        int level;
        const char* log_file;
        infra_log_callback_t callback;
        infra_mutex_t mutex;
    } log;

    // 数据结构状态
    struct {
        size_t hash_initial_size;
        uint32_t hash_load_factor;
    } ds;
} g_infra = {0};

//-----------------------------------------------------------------------------
// Default Configuration
//-----------------------------------------------------------------------------

const infra_config_t INFRA_DEFAULT_CONFIG = {
    .memory = {
        .use_memory_pool = false,
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = sizeof(void*)
    },
    .log = {
        .level = INFRA_LOG_LEVEL_INFO,
        .log_file = NULL
    },
    .ds = {
        .hash_initial_size = 16,
        .hash_load_factor = 75  // 75%
    }
};

//-----------------------------------------------------------------------------
// Configuration Management
//-----------------------------------------------------------------------------

infra_error_t infra_config_init(infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    *config = INFRA_DEFAULT_CONFIG;
    return INFRA_OK;
}

infra_error_t infra_config_validate(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 验证内存配置
    if (config->memory.use_memory_pool) {
        if (config->memory.pool_initial_size == 0 || 
            config->memory.pool_alignment == 0 || 
            config->memory.pool_alignment < sizeof(void*) || 
            (config->memory.pool_alignment & (config->memory.pool_alignment - 1)) != 0) {
            return INFRA_ERROR_INVALID_PARAM;
        }
    }

    // 验证日志配置
    if (config->log.level < INFRA_LOG_LEVEL_NONE || 
        config->log.level > INFRA_LOG_LEVEL_TRACE) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 验证数据结构配置
    if (config->ds.hash_initial_size == 0 || 
        config->ds.hash_load_factor == 0 || 
        config->ds.hash_load_factor > 100) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    return INFRA_OK;
}

infra_error_t infra_config_apply(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_error_t err = infra_config_validate(config);
    if (err != INFRA_OK) {
        return err;
    }

    // 应用日志配置
    g_infra.log.level = config->log.level;
    g_infra.log.log_file = config->log.log_file;

    // 应用数据结构配置
    g_infra.ds.hash_initial_size = config->ds.hash_initial_size;
    g_infra.ds.hash_load_factor = config->ds.hash_load_factor;

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Initialization and Cleanup
//-----------------------------------------------------------------------------

static infra_error_t init_module(infra_init_flags_t flag, const infra_config_t* config) {
    infra_error_t err = INFRA_OK;

    switch (flag) {
        case INFRA_INIT_MEMORY:
            err = infra_memory_init(&config->memory);
            break;

        case INFRA_INIT_LOG:
            // 初始化日志系统
            g_infra.log.level = config->log.level;
            g_infra.log.log_file = config->log.log_file;
            break;

        case INFRA_INIT_DS:
            // 初始化数据结构
            g_infra.ds.hash_initial_size = config->ds.hash_initial_size;
            g_infra.ds.hash_load_factor = config->ds.hash_load_factor;
            break;

        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    return err;
}

infra_error_t infra_init_with_config(infra_init_flags_t flags, const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 检查是否已经初始化
    if (g_infra.initialized) {
        return INFRA_ERROR_EXISTS;
    }

    // 创建全局互斥锁
    infra_error_t err = infra_mutex_create(&g_infra.mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // 如果需要日志系统，必须先初始化内存系统
    if ((flags & INFRA_INIT_LOG) && !(flags & INFRA_INIT_MEMORY)) {
        flags |= INFRA_INIT_MEMORY;
    }

    // 初始化内存管理
    if (flags & INFRA_INIT_MEMORY) {
        err = infra_memory_init(&config->memory);
        if (err != INFRA_OK) {
            infra_mutex_destroy(g_infra.mutex);
            return err;
        }
        g_infra.active_flags |= INFRA_INIT_MEMORY;
    }

    // 初始化日志系统
    if (flags & INFRA_INIT_LOG) {
        err = infra_mutex_create(&g_infra.log.mutex);
        if (err != INFRA_OK) {
            if (g_infra.active_flags & INFRA_INIT_MEMORY) {
                infra_memory_cleanup();
            }
            infra_mutex_destroy(g_infra.mutex);
            return err;
        }
        
        g_infra.log.level = config->log.level;
        g_infra.log.log_file = config->log.log_file;
        g_infra.log.callback = NULL;
        
        g_infra.active_flags |= INFRA_INIT_LOG;
    }

    // 初始化数据结构
    if (flags & INFRA_INIT_DS) {
        if (!(g_infra.active_flags & INFRA_INIT_MEMORY)) {
            if (g_infra.active_flags & INFRA_INIT_LOG) {
                infra_mutex_destroy(g_infra.log.mutex);
            }
            infra_mutex_destroy(g_infra.mutex);
            return INFRA_ERROR_DEPENDENCY;
        }
        
        g_infra.ds.hash_initial_size = config->ds.hash_initial_size;
        g_infra.ds.hash_load_factor = config->ds.hash_load_factor;
        g_infra.active_flags |= INFRA_INIT_DS;
    }

    g_infra.initialized = true;
    return INFRA_OK;
}

infra_error_t infra_init(void) {
    // 使用默认配置初始化
    infra_config_t config;
    infra_error_t err = infra_config_init(&config);
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize config: %d\n", err);
        return err;
    }

    // 验证配置
    err = infra_config_validate(&config);
    if (err != INFRA_OK) {
        infra_printf("Failed to validate config: %d\n", err);
        infra_printf("Memory config: use_pool=%d, size=%zu, align=%zu\n",
                    config.memory.use_memory_pool,
                    config.memory.pool_initial_size,
                    config.memory.pool_alignment);
        infra_printf("Log config: level=%d, file=%s\n",
                    config.log.level,
                    config.log.log_file ? config.log.log_file : "NULL");
        infra_printf("DS config: hash_size=%zu, load_factor=%u\n",
                    config.ds.hash_initial_size,
                    config.ds.hash_load_factor);
        return err;
    }

    // 初始化所有模块
    err = infra_init_with_config(INFRA_INIT_ALL, &config);
    if (err != INFRA_OK) {
        infra_printf("Failed to initialize with config: %d\n", err);
    }
    return err;
}

void infra_cleanup(void) {
    if (!g_infra.initialized) {
        return;
    }

    // 按照初始化的相反顺序清理
    if (g_infra.active_flags & INFRA_INIT_DS) {
        // 清理数据结构
        g_infra.active_flags &= ~INFRA_INIT_DS;
    }

    if (g_infra.active_flags & INFRA_INIT_LOG) {
        // 清理日志系统
        g_infra.active_flags &= ~INFRA_INIT_LOG;
    }

    if (g_infra.active_flags & INFRA_INIT_MEMORY) {
        // 清理内存管理
        infra_memory_cleanup();
        g_infra.active_flags &= ~INFRA_INIT_MEMORY;
    }

    // 清理全局互斥锁
    infra_mutex_destroy(g_infra.mutex);

    // 重置全局状态
    g_infra.initialized = false;
    g_infra.active_flags = 0;
}

bool infra_is_initialized(infra_init_flags_t flag) {
    return g_infra.initialized && (g_infra.active_flags & flag) == flag;
}

//-----------------------------------------------------------------------------
// Status Management
//-----------------------------------------------------------------------------

infra_error_t infra_get_status(infra_status_t* status) {
    if (!status) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    status->initialized = g_infra.initialized;
    status->active_flags = g_infra.active_flags;
    
    // 获取内存统计信息
    if (g_infra.active_flags & INFRA_INIT_MEMORY) {
        infra_memory_get_stats(&status->memory);
    } else {
        memset(&status->memory, 0, sizeof(infra_memory_stats_t));
    }

    status->log.log_entries = 0;  // TODO: 实现日志统计
    status->log.dropped_entries = 0;  // TODO: 实现日志统计

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------

// 内存管理函数已迁移到 infra_memory.c

//-----------------------------------------------------------------------------
// String Operations
//-----------------------------------------------------------------------------

size_t infra_strlen(const char* s) {
    return strlen(s);
}

char* infra_strcpy(char* dest, const char* src) {
    return strcpy(dest, src);
}

char* infra_strncpy(char* dest, const char* src, size_t n) {
    return strncpy(dest, src, n);
}

char* infra_strcat(char* dest, const char* src) {
    return strcat(dest, src);
}

char* infra_strncat(char* dest, const char* src, size_t n) {
    return strncat(dest, src, n);
}

int infra_strcmp(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}

int infra_strncmp(const char* s1, const char* s2, size_t n) {
    return strncmp(s1, s2, n);
}

char* infra_strdup(const char* s) {
    size_t len = infra_strlen(s) + 1;
    char* new_str = infra_malloc(len);
    if (new_str) {
        infra_memcpy(new_str, s, len);
    }
    return new_str;
}

char* infra_strndup(const char* s, size_t n) {
    size_t len = infra_strlen(s);
    if (len > n) len = n;
    char* new_str = infra_malloc(len + 1);
    if (new_str) {
        infra_memcpy(new_str, s, len);
        new_str[len] = '\0';
    }
    return new_str;
}

char* infra_strchr(const char* s, int c) {
    return strchr(s, c);
}

char* infra_strrchr(const char* s, int c) {
    return strrchr(s, c);
}

char* infra_strstr(const char* haystack, const char* needle) {
    return strstr(haystack, needle);
}

//-----------------------------------------------------------------------------
// Buffer Operations
//-----------------------------------------------------------------------------

infra_error_t infra_buffer_init(infra_buffer_t* buf, size_t initial_capacity) {
    if (!buf || initial_capacity == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    buf->data = infra_malloc(initial_capacity);
    if (!buf->data) {
        return INFRA_ERROR_NO_MEMORY;
    }
    buf->size = 0;
    buf->capacity = initial_capacity;
    return INFRA_OK;
}

void infra_buffer_destroy(infra_buffer_t* buf) {
    if (buf && buf->data) {
        infra_free(buf->data);
        buf->data = NULL;
        buf->size = 0;
        buf->capacity = 0;
    }
}

infra_error_t infra_buffer_reserve(infra_buffer_t* buf, size_t capacity) {
    if (!buf) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (capacity <= buf->capacity) {
        return INFRA_OK;
    }
    uint8_t* new_data = infra_realloc(buf->data, capacity);
    if (!new_data) {
        return INFRA_ERROR_NO_MEMORY;
    }
    buf->data = new_data;
    buf->capacity = capacity;
    return INFRA_OK;
}

infra_error_t infra_buffer_write(infra_buffer_t* buf, const void* data, size_t size) {
    if (!buf || !data || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (buf->size + size > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        if (new_capacity < buf->size + size) {
            new_capacity = buf->size + size;
        }
        infra_error_t err = infra_buffer_reserve(buf, new_capacity);
        if (err != INFRA_OK) {
            return err;
        }
    }
    infra_memcpy(buf->data + buf->size, data, size);
    buf->size += size;
    return INFRA_OK;
}

infra_error_t infra_buffer_read(infra_buffer_t* buf, void* data, size_t size) {
    if (!buf || !data || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (size > buf->size) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    infra_memcpy(data, buf->data, size);
    return INFRA_OK;
}

size_t infra_buffer_readable(const infra_buffer_t* buf) {
    return buf ? buf->size : 0;
}

size_t infra_buffer_writable(const infra_buffer_t* buf) {
    return buf ? buf->capacity - buf->size : 0;
}

void infra_buffer_reset(infra_buffer_t* buf) {
    if (buf) {
        buf->size = 0;
    }
}

//-----------------------------------------------------------------------------
// Logging
//-----------------------------------------------------------------------------

void infra_log_set_level(int level) {
    if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
        g_infra.log.level = level;
    }
}

void infra_log_set_callback(infra_log_callback_t callback) {
    g_infra.log.callback = callback;
}

void infra_log(int level, const char* file, int line, const char* func,
               const char* format, ...) {
    if (!g_infra.initialized || level > g_infra.log.level || !format) {
        return;
    }

    char message[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (g_infra.log.callback) {
        g_infra.log.callback(level, file, line, func, message);
    } else {
        const char* level_str = "UNKNOWN";
        switch (level) {
            case INFRA_LOG_LEVEL_ERROR: level_str = "ERROR"; break;
            case INFRA_LOG_LEVEL_WARN:  level_str = "WARN"; break;
            case INFRA_LOG_LEVEL_INFO:  level_str = "INFO"; break;
            case INFRA_LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
            case INFRA_LOG_LEVEL_TRACE: level_str = "TRACE"; break;
        }
        
        infra_mutex_lock(g_infra.log.mutex);
        fprintf(stderr, "[%s] %s:%d %s(): %s\n", level_str, file, line, func, message);
        fflush(stderr);
        infra_mutex_unlock(g_infra.log.mutex);
    }
}

//-----------------------------------------------------------------------------
// I/O Operations
//-----------------------------------------------------------------------------

infra_error_t infra_printf(const char* format, ...) {
    if (!format) {
        return INFRA_ERROR_INVALID;
    }
    
    va_list args;
    va_start(args, format);
    int result = vfprintf(stdout, format, args);
    va_end(args);
    fflush(stdout);
    
    return (result >= 0) ? INFRA_OK : INFRA_ERROR_IO;
}

//-----------------------------------------------------------------------------
// File Operations
//-----------------------------------------------------------------------------

infra_error_t infra_file_close(infra_handle_t handle) {
    if (!handle) return INFRA_ERROR_INVALID;

    if (close((int)handle) == -1) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_file_read(infra_handle_t handle, void* buffer, size_t size, size_t* bytes_read) {
    if (!handle || !buffer || !bytes_read) return INFRA_ERROR_INVALID;

    size_t total_read = 0;
    uint8_t* buf = (uint8_t*)buffer;

    while (total_read < size) {
        ssize_t result = read((int)handle, buf + total_read, size - total_read);
        if (result == -1) {
            if (errno == EINTR) continue;  // 被信号中断，重试
            return INFRA_ERROR_IO;
        }
        if (result == 0) break;  // EOF
        total_read += result;
    }

    *bytes_read = total_read;
    return INFRA_OK;
}

infra_error_t infra_file_write(infra_handle_t handle, const void* buffer, size_t size, size_t* bytes_written) {
    if (!handle || !buffer || !bytes_written) return INFRA_ERROR_INVALID;

    size_t total_written = 0;
    const uint8_t* buf = (const uint8_t*)buffer;

    while (total_written < size) {
        ssize_t result = write((int)handle, buf + total_written, size - total_written);
        if (result == -1) {
            if (errno == EINTR) continue;  // 被信号中断，重试
            return INFRA_ERROR_IO;
        }
        total_written += result;
    }

    *bytes_written = total_written;
    return INFRA_OK;
}

infra_error_t infra_file_seek(infra_handle_t handle, int64_t offset, int whence) {
    if (!handle) return INFRA_ERROR_INVALID;

    int os_whence;
    switch (whence) {
        case INFRA_SEEK_SET: os_whence = SEEK_SET; break;
        case INFRA_SEEK_CUR: os_whence = SEEK_CUR; break;
        case INFRA_SEEK_END: os_whence = SEEK_END; break;
        default: return INFRA_ERROR_INVALID;
    }

    if (lseek((int)handle, offset, os_whence) == -1) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_file_size(infra_handle_t handle, size_t* size) {
    if (!handle || !size) return INFRA_ERROR_INVALID;

    // 保存当前位置
    off_t current = lseek((int)handle, 0, SEEK_CUR);
    if (current == -1) {
        return INFRA_ERROR_IO;
    }

    // 移动到文件末尾
    off_t end = lseek((int)handle, 0, SEEK_END);
    if (end == -1) {
        return INFRA_ERROR_IO;
    }

    // 恢复原位置
    if (lseek((int)handle, current, SEEK_SET) == -1) {
        return INFRA_ERROR_IO;
    }

    *size = (size_t)end;
    return INFRA_OK;
}

infra_error_t infra_file_remove(const char* path) {
    if (!path) return INFRA_ERROR_INVALID;

    if (unlink(path) == -1) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_file_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return INFRA_ERROR_INVALID;

    if (rename(old_path, new_path) == -1) {
        return INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

infra_error_t infra_file_exists(const char* path, bool* exists) {
    if (!path || !exists) return INFRA_ERROR_INVALID;

    struct stat st;
    int result = stat(path, &st);
    *exists = (result == 0);

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Time Management
//-----------------------------------------------------------------------------

infra_time_t infra_time_now(void) {
    infra_time_t time;
    infra_platform_get_time(&time);
    return time;
}

infra_time_t infra_time_monotonic(void) {
    infra_time_t time;
    infra_platform_get_monotonic_time(&time);
    return time;
}

void infra_time_sleep(uint32_t ms) {
    infra_platform_sleep(ms);
}

void infra_time_yield(void) {
    infra_platform_yield();
}

