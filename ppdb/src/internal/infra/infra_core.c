/*
 * infra.c - Infrastructure Layer Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_sync.h"
//#include "internal/infra/infra_async.h"

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

struct {
    bool initialized;
    infra_init_flags_t active_flags;
    infra_config_t config;
    infra_status_t status;
    infra_mutex_t mutex;
    struct {
        int level;
        infra_log_callback_t callback;
        infra_mutex_t mutex;
    } log;
} g_infra = {0};

//-----------------------------------------------------------------------------
// Default Configuration
//-----------------------------------------------------------------------------

const infra_config_t INFRA_DEFAULT_CONFIG = {
    .memory = {
        .use_memory_pool = false,
        .pool_initial_size = 1024 * 1024,  // 1MB
        .pool_alignment = 8
    },
    .log = {
        .level = INFRA_LOG_LEVEL_INFO,
        .buffer_size = 4096,
        .async_logging = true,
        .log_file = NULL
    },
    .ds = {
        .hash_initial_size = 16,
        .hash_load_factor = 75,  // 75%
        .thread_safe = true
    }
};

//-----------------------------------------------------------------------------
// Configuration Management
//-----------------------------------------------------------------------------

infra_error_t infra_config_init(infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID;
    }
    *config = INFRA_DEFAULT_CONFIG;
    return INFRA_OK;
}

infra_error_t infra_config_validate(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID;
    }

    // 验证内存配置
    if (config->memory.pool_initial_size == 0 ||
        config->memory.pool_alignment == 0 ||
        (config->memory.pool_alignment & (config->memory.pool_alignment - 1)) != 0) {
        return INFRA_ERROR_INVALID;
    }

    // 验证日志配置
    if (config->log.level < INFRA_LOG_LEVEL_NONE ||
        config->log.level > INFRA_LOG_LEVEL_TRACE ||
        config->log.buffer_size == 0) {
        return INFRA_ERROR_INVALID;
    }

    // 验证数据结构配置
    if (config->ds.hash_initial_size == 0 ||
        config->ds.hash_load_factor == 0 ||
        config->ds.hash_load_factor > 100) {
        return INFRA_ERROR_INVALID;
    }

    // 验证任务分类配置
    if (config->async.classify.io_threshold_us >= config->async.classify.cpu_threshold_us ||
        config->async.classify.sample_window == 0) {
        return INFRA_ERROR_INVALID;
    }

    return INFRA_OK;
}

infra_error_t infra_config_apply(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID;
    }

    infra_error_t err = infra_config_validate(config);
    if (err != INFRA_OK) {
        return err;
    }

    if (g_infra.initialized) {
        return INFRA_ERROR_BUSY;  // 不能在运行时修改配置
    }

    g_infra.config = *config;
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Initialization and Cleanup
//-----------------------------------------------------------------------------

static infra_error_t init_module(infra_init_flags_t flag) {
    infra_error_t err = INFRA_OK;

    switch (flag) {
        case INFRA_INIT_MEMORY:
            // 初始化内存管理
            if (g_infra.config.memory.use_memory_pool) {
                // TODO: 实现内存池
            }
            break;

        case INFRA_INIT_ASYNC:
            // 初始化异步系统
            // TODO: 使用配置的线程数和队列大小
            break;

        case INFRA_INIT_LOG:
            err = infra_mutex_create(&g_infra.log.mutex);
            if (err != INFRA_OK) {
                return err;
            }
            g_infra.log.level = g_infra.config.log.level;
            g_infra.log.callback = NULL;
            break;

        case INFRA_INIT_DS:
            // 初始化数据结构
            // TODO: 应用数据结构配置
            break;

        default:
            return INFRA_ERROR_INVALID;
    }

    if (err == INFRA_OK) {
        g_infra.active_flags |= flag;
    }

    return err;
}

infra_error_t infra_init_with_config(infra_init_flags_t flags, const infra_config_t* config) {
    if (g_infra.initialized) {
        return INFRA_ERROR_BUSY;
    }

    // 应用配置
    infra_error_t err = INFRA_OK;
    if (config) {
        err = infra_config_apply(config);
        if (err != INFRA_OK) {
            return err;
        }
    } else {
        g_infra.config = INFRA_DEFAULT_CONFIG;
    }

    // 创建全局锁
    err = infra_mutex_create(&g_infra.mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // 按顺序初始化各个模块
    if (flags & INFRA_INIT_MEMORY) {
        err = init_module(INFRA_INIT_MEMORY);
        if (err != INFRA_OK) goto cleanup;
    }

    if (flags & INFRA_INIT_LOG) {
        err = init_module(INFRA_INIT_LOG);
        if (err != INFRA_OK) goto cleanup;
    }

    if (flags & INFRA_INIT_ASYNC) {
        err = init_module(INFRA_INIT_ASYNC);
        if (err != INFRA_OK) goto cleanup;
    }

    if (flags & INFRA_INIT_DS) {
        err = init_module(INFRA_INIT_DS);
        if (err != INFRA_OK) goto cleanup;
    }

    g_infra.initialized = true;
    return INFRA_OK;

cleanup:
    infra_cleanup();
    return err;
}

infra_error_t infra_init(void) {
    return infra_init_with_config(INFRA_INIT_ALL, NULL);
}

void infra_cleanup(void) {
    if (!g_infra.initialized) {
        return;
    }

    // 按相反顺序清理各个模块
    if (g_infra.active_flags & INFRA_INIT_DS) {
        // TODO: 清理数据结构
    }

    if (g_infra.active_flags & INFRA_INIT_ASYNC) {
        // TODO: 清理异步系统
    }

    if (g_infra.active_flags & INFRA_INIT_LOG) {
        // TODO: 清理日志系统
    }

    if (g_infra.active_flags & INFRA_INIT_MEMORY) {
        // TODO: 清理内存管理
    }

    infra_mutex_destroy(g_infra.mutex);
    memset(&g_infra, 0, sizeof(g_infra));
}

bool infra_is_initialized(infra_init_flags_t flag) {
    return g_infra.initialized && (g_infra.active_flags & flag) == flag;
}

//-----------------------------------------------------------------------------
// Status Management
//-----------------------------------------------------------------------------

infra_error_t infra_get_status(infra_status_t* status) {
    if (!status) {
        return INFRA_ERROR_INVALID;
    }

    infra_mutex_lock(g_infra.mutex);
    *status = g_infra.status;
    status->initialized = g_infra.initialized;
    status->active_flags = g_infra.active_flags;
    infra_mutex_unlock(g_infra.mutex);

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Error Handling
//-----------------------------------------------------------------------------

const char* infra_error_string(int error_code) {
    switch (error_code) {
        case INFRA_OK:                  return "Success";
        case INFRA_ERROR_INVALID:       return "Invalid parameter";
        case INFRA_ERROR_MEMORY:        return "Memory error";
        case INFRA_ERROR_TIMEOUT:       return "Timeout";
        case INFRA_ERROR_BUSY:          return "Resource busy";
        case INFRA_ERROR_NOT_FOUND:     return "Not found";
        case INFRA_ERROR_EXISTS:        return "Already exists";
        case INFRA_ERROR_IO:           return "I/O error";
        case INFRA_ERROR_CANCELLED:    return "Operation cancelled";
        case INFRA_ERROR_NOT_READY:    return "Not ready";
        case INFRA_ERROR_FULL:        return "Resource full";
        case INFRA_ERROR_EMPTY:       return "Resource empty";
        case INFRA_ERROR_INVALID_PARAM: return "Invalid parameter";
        case INFRA_ERROR_NO_MEMORY:    return "Out of memory";
        default:                       return "Unknown error";
    }
}

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------

void* infra_malloc(size_t size) {
    return malloc(size);
}

void* infra_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void* infra_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

void infra_free(void* ptr) {
    free(ptr);
}

void* infra_memset(void* s, int c, size_t n) {
    return memset(s, c, n);
}

void* infra_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}

void* infra_memmove(void* dest, const void* src, size_t n) {
    return memmove(dest, src, n);
}

int infra_memcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

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
        return INFRA_ERROR_INVALID;
    }
    buf->data = infra_malloc(initial_capacity);
    if (!buf->data) {
        return INFRA_ERROR_MEMORY;
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
        return INFRA_ERROR_INVALID;
    }
    if (capacity <= buf->capacity) {
        return INFRA_OK;
    }
    uint8_t* new_data = infra_realloc(buf->data, capacity);
    if (!new_data) {
        return INFRA_ERROR_MEMORY;
    }
    buf->data = new_data;
    buf->capacity = capacity;
    return INFRA_OK;
}

infra_error_t infra_buffer_write(infra_buffer_t* buf, const void* data, size_t size) {
    if (!buf || !data || size == 0) {
        return INFRA_ERROR_INVALID;
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
        return INFRA_ERROR_INVALID;
    }
    if (size > buf->size) {
        return INFRA_ERROR_INVALID;
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
// Statistics
//-----------------------------------------------------------------------------

void infra_stats_init(infra_stats_t* stats) {
    if (stats) {
        infra_memset(stats, 0, sizeof(infra_stats_t));
        stats->min_latency_us = (uint64_t)-1;
    }
}

void infra_stats_reset(infra_stats_t* stats) {
    infra_stats_init(stats);
}

void infra_stats_update(infra_stats_t* stats, bool success, uint64_t latency_us,
                       size_t bytes, infra_error_t error) {
    if (!stats) return;

    stats->total_operations++;
    if (success) {
        stats->successful_operations++;
    } else {
        stats->failed_operations++;
        stats->last_error = error;
        stats->last_error_time = time(NULL);
    }

    stats->total_bytes += bytes;

    if (latency_us < stats->min_latency_us) {
        stats->min_latency_us = latency_us;
    }
    if (latency_us > stats->max_latency_us) {
        stats->max_latency_us = latency_us;
    }

    // Update average latency
    uint64_t total = stats->avg_latency_us * (stats->total_operations - 1);
    total += latency_us;
    stats->avg_latency_us = total / stats->total_operations;
}

void infra_stats_merge(infra_stats_t* dest, const infra_stats_t* src) {
    if (!dest || !src) return;

    dest->total_operations += src->total_operations;
    dest->successful_operations += src->successful_operations;
    dest->failed_operations += src->failed_operations;
    dest->total_bytes += src->total_bytes;

    if (src->min_latency_us < dest->min_latency_us) {
        dest->min_latency_us = src->min_latency_us;
    }
    if (src->max_latency_us > dest->max_latency_us) {
        dest->max_latency_us = src->max_latency_us;
    }

    // Update average latency
    uint64_t total1 = dest->avg_latency_us * (dest->total_operations - src->total_operations);
    uint64_t total2 = src->avg_latency_us * src->total_operations;
    dest->avg_latency_us = (total1 + total2) / dest->total_operations;

    if (src->last_error_time > dest->last_error_time) {
        dest->last_error = src->last_error;
        dest->last_error_time = src->last_error_time;
    }
}

void infra_stats_print(const infra_stats_t* stats, const char* prefix) {
    if (!stats) return;
    printf("%sTotal operations: %lu\n", prefix ? prefix : "", (unsigned long)stats->total_operations);
    printf("%sSuccessful operations: %lu\n", prefix ? prefix : "", (unsigned long)stats->successful_operations);
    printf("%sFailed operations: %lu\n", prefix ? prefix : "", (unsigned long)stats->failed_operations);
    printf("%sTotal bytes: %lu\n", prefix ? prefix : "", (unsigned long)stats->total_bytes);
    printf("%sMin latency: %lu us\n", prefix ? prefix : "", (unsigned long)stats->min_latency_us);
    printf("%sMax latency: %lu us\n", prefix ? prefix : "", (unsigned long)stats->max_latency_us);
    printf("%sAvg latency: %lu us\n", prefix ? prefix : "", (unsigned long)stats->avg_latency_us);
    if (stats->last_error_time) {
        printf("%sLast error: %s at %lu\n", prefix ? prefix : "",
               infra_error_string(stats->last_error), (unsigned long)stats->last_error_time);
    }
}

//-----------------------------------------------------------------------------
// Data Structures
//-----------------------------------------------------------------------------

// List Implementation
infra_error_t infra_list_create(infra_list_t** list) {
    if (!list) {
        return INFRA_ERROR_INVALID;
    }
    *list = infra_malloc(sizeof(infra_list_t));
    if (!*list) {
        return INFRA_ERROR_MEMORY;
    }
    (*list)->head = NULL;
    (*list)->tail = NULL;
    (*list)->size = 0;
    return INFRA_OK;
}

void infra_list_destroy(infra_list_t* list) {
    if (!list) {
        return;
    }
    infra_list_node_t* current = list->head;
    while (current) {
        infra_list_node_t* next = current->next;
        infra_free(current);
        current = next;
    }
    infra_free(list);
}

infra_error_t infra_list_append(infra_list_t* list, void* value) {
    if (!list) {
        return INFRA_ERROR_INVALID;
    }
    infra_list_node_t* node = infra_malloc(sizeof(infra_list_node_t));
    if (!node) {
        return INFRA_ERROR_MEMORY;
    }
    node->value = value;
    node->next = NULL;
    node->prev = list->tail;
    
    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->size++;
    return INFRA_OK;
}

infra_error_t infra_list_remove(infra_list_t* list, infra_list_node_t* node) {
    if (!list || !node) {
        return INFRA_ERROR_INVALID;
    }
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }
    infra_free(node);
    list->size--;
    return INFRA_OK;
}

infra_list_node_t* infra_list_head(infra_list_t* list) {
    return list ? list->head : NULL;
}

infra_list_node_t* infra_list_node_next(infra_list_node_t* node) {
    return node ? node->next : NULL;
}

void* infra_list_node_value(infra_list_node_t* node) {
    return node ? node->value : NULL;
}

// Hash Table Implementation
infra_error_t infra_hash_create(infra_hash_t** hash, size_t capacity) {
    if (!hash || capacity == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *hash = infra_malloc(sizeof(infra_hash_t));
    if (!*hash) {
        return INFRA_ERROR_NO_MEMORY;
    }

    (*hash)->buckets = infra_calloc(capacity, sizeof(infra_hash_node_t*));
    if (!(*hash)->buckets) {
        infra_free(*hash);
        *hash = NULL;
        return INFRA_ERROR_NO_MEMORY;
    }

    (*hash)->size = 0;
    (*hash)->capacity = capacity;
    return INFRA_OK;
}

void infra_hash_destroy(infra_hash_t* hash) {
    if (!hash) {
        return;
    }

    for (size_t i = 0; i < hash->capacity; i++) {
        infra_hash_node_t* current = hash->buckets[i];
        while (current) {
            infra_hash_node_t* next = current->next;
            infra_free(current->key);
            infra_free(current);
            current = next;
        }
    }

    infra_free(hash->buckets);
    infra_free(hash);
}

static size_t hash_function(const char* key) {
    size_t hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

infra_error_t infra_hash_put(infra_hash_t* hash, const char* key, void* value) {
    if (!hash || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t index = hash_function(key) % hash->capacity;
    infra_hash_node_t* node = hash->buckets[index];

    while (node) {
        if (infra_strcmp(node->key, key) == 0) {
            node->value = value;
            return INFRA_OK;
        }
        node = node->next;
    }

    node = infra_malloc(sizeof(infra_hash_node_t));
    if (!node) {
        return INFRA_ERROR_NO_MEMORY;
    }

    node->key = infra_strdup(key);
    if (!node->key) {
        infra_free(node);
        return INFRA_ERROR_NO_MEMORY;
    }

    node->value = value;
    node->next = hash->buckets[index];
    hash->buckets[index] = node;
    hash->size++;

    return INFRA_OK;
}

void* infra_hash_get(infra_hash_t* hash, const char* key) {
    if (!hash || !key) {
        return NULL;
    }

    size_t index = hash_function(key) % hash->capacity;
    infra_hash_node_t* node = hash->buckets[index];

    while (node) {
        if (infra_strcmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }

    return NULL;
}

void* infra_hash_remove(infra_hash_t* hash, const char* key) {
    if (!hash || !key) {
        return NULL;
    }

    size_t index = hash_function(key) % hash->capacity;
    infra_hash_node_t* node = hash->buckets[index];
    infra_hash_node_t* prev = NULL;

    while (node) {
        if (infra_strcmp(node->key, key) == 0) {
            void* value = node->value;
            if (prev) {
                prev->next = node->next;
            } else {
                hash->buckets[index] = node->next;
            }
            infra_free(node->key);
            infra_free(node);
            hash->size--;
            return value;
        }
        prev = node;
        node = node->next;
    }

    return NULL;
}

void infra_hash_clear(infra_hash_t* hash) {
    if (!hash) {
        return;
    }

    for (size_t i = 0; i < hash->capacity; i++) {
        infra_hash_node_t* current = hash->buckets[i];
        while (current) {
            infra_hash_node_t* next = current->next;
            infra_free(current->key);
            infra_free(current);
            current = next;
        }
        hash->buckets[i] = NULL;
    }
    hash->size = 0;
}

// Red-Black Tree Implementation
static infra_rbtree_node_t* rbtree_create_node(int key, void* value) {
    infra_rbtree_node_t* node = infra_malloc(sizeof(infra_rbtree_node_t));
    if (node) {
        node->key = key;
        node->value = value;
        node->color = INFRA_RBTREE_RED;
        node->left = node->right = node->parent = NULL;
    }
    return node;
}

static void rbtree_rotate_left(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    infra_rbtree_node_t* right = node->right;
    node->right = right->left;
    if (right->left) {
        right->left->parent = node;
    }
    right->parent = node->parent;
    if (!node->parent) {
        tree->root = right;
    } else if (node == node->parent->left) {
        node->parent->left = right;
    } else {
        node->parent->right = right;
    }
    right->left = node;
    node->parent = right;
}

static void rbtree_rotate_right(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    infra_rbtree_node_t* left = node->left;
    node->left = left->right;
    if (left->right) {
        left->right->parent = node;
    }
    left->parent = node->parent;
    if (!node->parent) {
        tree->root = left;
    } else if (node == node->parent->right) {
        node->parent->right = left;
    } else {
        node->parent->left = left;
    }
    left->right = node;
    node->parent = left;
}

static void rbtree_fix_insert(infra_rbtree_t* tree, infra_rbtree_node_t* node) {
    while (node != tree->root && node->parent->color == INFRA_RBTREE_RED) {
        if (node->parent == node->parent->parent->left) {
            infra_rbtree_node_t* uncle = node->parent->parent->right;
            if (uncle && uncle->color == INFRA_RBTREE_RED) {
                node->parent->color = INFRA_RBTREE_BLACK;
                uncle->color = INFRA_RBTREE_BLACK;
                node->parent->parent->color = INFRA_RBTREE_RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    rbtree_rotate_left(tree, node);
                }
                node->parent->color = INFRA_RBTREE_BLACK;
                node->parent->parent->color = INFRA_RBTREE_RED;
                rbtree_rotate_right(tree, node->parent->parent);
            }
        } else {
            infra_rbtree_node_t* uncle = node->parent->parent->left;
            if (uncle && uncle->color == INFRA_RBTREE_RED) {
                node->parent->color = INFRA_RBTREE_BLACK;
                uncle->color = INFRA_RBTREE_BLACK;
                node->parent->parent->color = INFRA_RBTREE_RED;
                node = node->parent->parent;
            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    rbtree_rotate_right(tree, node);
                }
                node->parent->color = INFRA_RBTREE_BLACK;
                node->parent->parent->color = INFRA_RBTREE_RED;
                rbtree_rotate_left(tree, node->parent->parent);
            }
        }
    }
    tree->root->color = INFRA_RBTREE_BLACK;
}

infra_error_t infra_rbtree_create(infra_rbtree_t** tree) {
    if (!tree) {
        return INFRA_ERROR_INVALID;
    }
    *tree = infra_malloc(sizeof(infra_rbtree_t));
    if (!*tree) {
        return INFRA_ERROR_MEMORY;
    }
    (*tree)->root = NULL;
    (*tree)->size = 0;
    return INFRA_OK;
}

static void rbtree_destroy_recursive(infra_rbtree_node_t* node) {
    if (node) {
        rbtree_destroy_recursive(node->left);
        rbtree_destroy_recursive(node->right);
        infra_free(node);
    }
}

void infra_rbtree_destroy(infra_rbtree_t* tree) {
    if (tree) {
        rbtree_destroy_recursive(tree->root);
        infra_free(tree);
    }
}

infra_error_t infra_rbtree_insert(infra_rbtree_t* tree, int key, void* value) {
    if (!tree) {
        return INFRA_ERROR_INVALID;
    }
    infra_rbtree_node_t* node = rbtree_create_node(key, value);
    if (!node) {
        return INFRA_ERROR_MEMORY;
    }
    infra_rbtree_node_t* y = NULL;
    infra_rbtree_node_t* x = tree->root;
    while (x) {
        y = x;
        if (key < x->key) {
            x = x->left;
        } else if (key > x->key) {
            x = x->right;
        } else {
            x->value = value;
            infra_free(node);
            return INFRA_OK;
        }
    }
    node->parent = y;
    if (!y) {
        tree->root = node;
    } else if (key < y->key) {
        y->left = node;
    } else {
        y->right = node;
    }
    tree->size++;
    rbtree_fix_insert(tree, node);
    return INFRA_OK;
}

static infra_rbtree_node_t* rbtree_find_node(infra_rbtree_t* tree, int key) {
    infra_rbtree_node_t* node = tree->root;
    while (node) {
        if (key < node->key) {
            node = node->left;
        } else if (key > node->key) {
            node = node->right;
        } else {
            return node;
        }
    }
    return NULL;
}

void* infra_rbtree_find(infra_rbtree_t* tree, int key) {
    if (!tree) {
        return NULL;
    }
    infra_rbtree_node_t* node = rbtree_find_node(tree, key);
    return node ? node->value : NULL;
}

static void rbtree_transplant(infra_rbtree_t* tree, infra_rbtree_node_t* u, infra_rbtree_node_t* v) {
    if (!u->parent) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    if (v) {
        v->parent = u->parent;
    }
}

static infra_rbtree_node_t* rbtree_minimum(infra_rbtree_node_t* node) {
    while (node->left) {
        node = node->left;
    }
    return node;
}

static void rbtree_fix_delete(infra_rbtree_t* tree, infra_rbtree_node_t* x, infra_rbtree_node_t* parent) {
    while (x != tree->root && (!x || x->color == INFRA_RBTREE_BLACK)) {
        if (x == parent->left) {
            infra_rbtree_node_t* w = parent->right;
            if (w->color == INFRA_RBTREE_RED) {
                w->color = INFRA_RBTREE_BLACK;
                parent->color = INFRA_RBTREE_RED;
                rbtree_rotate_left(tree, parent);
                w = parent->right;
            }
            if ((!w->left || w->left->color == INFRA_RBTREE_BLACK) &&
                (!w->right || w->right->color == INFRA_RBTREE_BLACK)) {
                w->color = INFRA_RBTREE_RED;
                x = parent;
                parent = x->parent;
            } else {
                if (!w->right || w->right->color == INFRA_RBTREE_BLACK) {
                    if (w->left) {
                        w->left->color = INFRA_RBTREE_BLACK;
                    }
                    w->color = INFRA_RBTREE_RED;
                    rbtree_rotate_right(tree, w);
                    w = parent->right;
                }
                w->color = parent->color;
                parent->color = INFRA_RBTREE_BLACK;
                if (w->right) {
                    w->right->color = INFRA_RBTREE_BLACK;
                }
                rbtree_rotate_left(tree, parent);
                x = tree->root;
                break;
            }
        } else {
            infra_rbtree_node_t* w = parent->left;
            if (w->color == INFRA_RBTREE_RED) {
                w->color = INFRA_RBTREE_BLACK;
                parent->color = INFRA_RBTREE_RED;
                rbtree_rotate_right(tree, parent);
                w = parent->left;
            }
            if ((!w->right || w->right->color == INFRA_RBTREE_BLACK) &&
                (!w->left || w->left->color == INFRA_RBTREE_BLACK)) {
                w->color = INFRA_RBTREE_RED;
                x = parent;
                parent = x->parent;
            } else {
                if (!w->left || w->left->color == INFRA_RBTREE_BLACK) {
                    if (w->right) {
                        w->right->color = INFRA_RBTREE_BLACK;
                    }
                    w->color = INFRA_RBTREE_RED;
                    rbtree_rotate_left(tree, w);
                    w = parent->left;
                }
                w->color = parent->color;
                parent->color = INFRA_RBTREE_BLACK;
                if (w->right) {
                    w->right->color = INFRA_RBTREE_BLACK;
                }
                rbtree_rotate_right(tree, parent);
                x = tree->root;
                break;
            }
        }
    }
    if (x) {
        x->color = INFRA_RBTREE_BLACK;
    }
}

void* infra_rbtree_remove(infra_rbtree_t* tree, int key) {
    if (!tree) {
        return NULL;
    }
    infra_rbtree_node_t* z = rbtree_find_node(tree, key);
    if (!z) {
        return NULL;
    }
    void* value = z->value;
    infra_rbtree_node_t* y = z;
    infra_rbtree_node_t* x;
    infra_rbtree_color_t y_original_color = y->color;
    
    if (!z->left) {
        x = z->right;
        rbtree_transplant(tree, z, z->right);
    } else if (!z->right) {
        x = z->left;
        rbtree_transplant(tree, z, z->left);
    } else {
        y = rbtree_minimum(z->right);
        y_original_color = y->color;
        x = y->right;
        if (y->parent == z) {
            if (x) {
                x->parent = y;
            }
        } else {
            rbtree_transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        rbtree_transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }
    
    if (y_original_color == INFRA_RBTREE_BLACK) {
        rbtree_fix_delete(tree, x, x ? x->parent : NULL);
    }
    
    infra_free(z);
    tree->size--;
    return value;
}

void infra_rbtree_clear(infra_rbtree_t* tree) {
    if (tree) {
        rbtree_destroy_recursive(tree->root);
        tree->root = NULL;
        tree->size = 0;
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

infra_error_t infra_file_open(const char* path, infra_flags_t flags, int mode, infra_handle_t* handle) {
    if (!path || !handle) return INFRA_ERROR_INVALID;

    int os_flags = 0;
    
    // 基本访问模式
    if ((flags & INFRA_FILE_RDWR) == INFRA_FILE_RDWR) {
        os_flags |= O_RDWR;
    } else if (flags & INFRA_FILE_WRONLY) {
        os_flags |= O_WRONLY;
    } else {
        os_flags |= O_RDONLY;  // 默认为只读
    }
    
    // 创建和修改标志
    if (flags & INFRA_FILE_CREATE) {
        os_flags |= O_CREAT;
        if (!(flags & INFRA_FILE_APPEND)) {
            os_flags |= O_TRUNC;  // 如果不是追加模式，则截断文件
        }
    }
    if (flags & INFRA_FILE_APPEND) os_flags |= O_APPEND;
    if (flags & INFRA_FILE_TRUNC) os_flags |= O_TRUNC;

    printf("Opening file %s with flags 0x%x (os_flags 0x%x) mode 0%o\n", 
           path, flags, os_flags, mode);
           
    int fd = open(path, os_flags, mode);
    if (fd == -1) {
        printf("Failed to open file: %s (errno: %d)\n", strerror(errno), errno);
        return INFRA_ERROR_IO;
    }

    *handle = (infra_handle_t)fd;
    return INFRA_OK;
}

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

//-----------------------------------------------------------------------------
// Skip List Implementation
//-----------------------------------------------------------------------------

// Helper function to create a new node
static infra_skiplist_node_t* create_node(size_t level, const void* key, size_t key_size, const void* value, size_t value_size) {
    infra_skiplist_node_t* node = (infra_skiplist_node_t*)infra_malloc(sizeof(infra_skiplist_node_t));
    if (!node) return NULL;

    node->key = infra_malloc(key_size);
    if (!node->key) {
        infra_free(node);
        return NULL;
    }

    node->value = infra_malloc(value_size);
    if (!node->value) {
        infra_free(node->key);
        infra_free(node);
        return NULL;
    }

    memcpy(node->key, key, key_size);
    memcpy(node->value, value, value_size);
    node->key_size = key_size;
    node->value_size = value_size;
    node->level = level;

    return node;
}

// Helper function to free a node
static void free_node(infra_skiplist_node_t* node) {
    if (node) {
        infra_free(node->key);
        infra_free(node->value);
        infra_free(node);
    }
}

// Helper function to generate a random level
static size_t random_level(void) {
    size_t level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF >> 1) && level < INFRA_SKIPLIST_MAX_LEVEL) {
        level++;
    }
    return level;
}

infra_error_t infra_skiplist_init(infra_skiplist_t* list, size_t max_level) {
    if (!list || max_level == 0 || max_level > INFRA_SKIPLIST_MAX_LEVEL) {
        return INFRA_ERROR_INVALID;
    }

    list->header = create_node(INFRA_SKIPLIST_MAX_LEVEL, NULL, 0, NULL, 0);
    if (!list->header) {
        return INFRA_ERROR_MEMORY;
    }

    for (size_t i = 0; i < INFRA_SKIPLIST_MAX_LEVEL; i++) {
        list->header->forward[i] = NULL;
    }

    list->level = 1;
    list->size = 0;
    list->compare = NULL;

    return INFRA_OK;
}

infra_error_t infra_skiplist_destroy(infra_skiplist_t* list) {
    if (!list) {
        return INFRA_ERROR_INVALID;
    }

    infra_skiplist_node_t* current = list->header;
    while (current) {
        infra_skiplist_node_t* next = current->forward[0];
        free_node(current);
        current = next;
    }

    list->header = NULL;
    list->level = 0;
    list->size = 0;
    list->compare = NULL;

    return INFRA_OK;
}

infra_error_t infra_skiplist_insert(infra_skiplist_t* list, const void* key, size_t key_size, const void* value, size_t value_size) {
    if (!list || !key || !value || key_size == 0 || value_size == 0 || !list->compare) {
        return INFRA_ERROR_INVALID;
    }

    infra_skiplist_node_t* update[INFRA_SKIPLIST_MAX_LEVEL];
    infra_skiplist_node_t* current = list->header;

    // Find position to insert
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    // Check if key already exists
    if (current && list->compare(current->key, key) == 0) {
        // Update value
        void* new_value = infra_malloc(value_size);
        if (!new_value) {
            return INFRA_ERROR_MEMORY;
        }

        memcpy(new_value, value, value_size);
        infra_free(current->value);
        current->value = new_value;
        current->value_size = value_size;

        return INFRA_OK;
    }

    // Insert new node
    size_t level = random_level();
    if (level > list->level) {
        for (size_t i = list->level; i < level; i++) {
            update[i] = list->header;
        }
        list->level = level;
    }

    infra_skiplist_node_t* node = create_node(level, key, key_size, value, value_size);
    if (!node) {
        return INFRA_ERROR_MEMORY;
    }

    for (size_t i = 0; i < level; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }

    list->size++;
    return INFRA_OK;
}

infra_error_t infra_skiplist_find(infra_skiplist_t* list, const void* key, size_t key_size, void** value, size_t* value_size) {
    if (!list || !key || !value || !value_size || key_size == 0 || !list->compare) {
        return INFRA_ERROR_INVALID;
    }

    infra_skiplist_node_t* current = list->header;

    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
    }

    current = current->forward[0];

    if (current && list->compare(current->key, key) == 0) {
        *value = current->value;
        *value_size = current->value_size;
        return INFRA_OK;
    }

    return INFRA_ERROR_NOT_FOUND;
}

infra_error_t infra_skiplist_remove(infra_skiplist_t* list, const void* key, size_t key_size) {
    if (!list || !key || key_size == 0 || !list->compare) {
        return INFRA_ERROR_INVALID;
    }

    infra_skiplist_node_t* update[INFRA_SKIPLIST_MAX_LEVEL];
    infra_skiplist_node_t* current = list->header;

    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && list->compare(current->forward[i]->key, key) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    if (current && list->compare(current->key, key) == 0) {
        for (size_t i = 0; i < list->level; i++) {
            if (update[i]->forward[i] != current) {
                break;
            }
            update[i]->forward[i] = current->forward[i];
        }

        while (list->level > 1 && list->header->forward[list->level - 1] == NULL) {
            list->level--;
        }

        free_node(current);
        list->size--;
        return INFRA_OK;
    }

    return INFRA_ERROR_NOT_FOUND;
}

infra_error_t infra_skiplist_size(infra_skiplist_t* list, size_t* size) {
    if (!list || !size) {
        return INFRA_ERROR_INVALID;
    }

    *size = list->size;
    return INFRA_OK;
}

infra_error_t infra_skiplist_clear(infra_skiplist_t* list) {
    if (!list) {
        return INFRA_ERROR_INVALID;
    }

    infra_skiplist_node_t* current = list->header->forward[0];
    while (current) {
        infra_skiplist_node_t* next = current->forward[0];
        free_node(current);
        current = next;
    }

    for (size_t i = 0; i < INFRA_SKIPLIST_MAX_LEVEL; i++) {
        list->header->forward[i] = NULL;
    }

    list->level = 1;
    list->size = 0;

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// String Operations
//-----------------------------------------------------------------------------

int infra_vsnprintf(char* str, size_t size, const char* format, va_list args) {
    return vsnprintf(str, size, format, args);
}

