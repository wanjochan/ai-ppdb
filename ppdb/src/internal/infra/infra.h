/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra.h - Infrastructure Layer
 */

#ifndef INFRA_H
#define INFRA_H

#include "cosmopolitan.h"

//-----------------------------------------------------------------------------
// Version Information
//-----------------------------------------------------------------------------

#define INFRA_VERSION_MAJOR 1
#define INFRA_VERSION_MINOR 0
#define INFRA_VERSION_PATCH 0

#define INFRA_VERSION_STRING "1.0.0"

//-----------------------------------------------------------------------------
// Basic Types
//-----------------------------------------------------------------------------

typedef int32_t infra_error_t;
typedef uint32_t infra_flags_t;
typedef uint64_t infra_time_t;
typedef uint64_t infra_handle_t;

//-----------------------------------------------------------------------------
// Thread Types
//-----------------------------------------------------------------------------

typedef void* infra_mutex_t;
typedef void* infra_cond_t;
typedef void* infra_thread_t;
typedef void* (*infra_thread_func_t)(void*);

//-----------------------------------------------------------------------------
// Error Codes
//-----------------------------------------------------------------------------

#define INFRA_OK                 0
#define INFRA_ERROR_INVALID     -1
#define INFRA_ERROR_MEMORY      -2
#define INFRA_ERROR_IO          -3
#define INFRA_ERROR_TIMEOUT     -4
#define INFRA_ERROR_BUSY        -5
#define INFRA_ERROR_NOTFOUND    -6
#define INFRA_ERROR_EXISTS      -7
#define INFRA_ERROR_FULL        -8
#define INFRA_ERROR_EMPTY       -9
#define INFRA_ERROR_AGAIN       -10
#define INFRA_ERROR_INTERRUPTED -11
#define INFRA_ERROR_CANCELLED   -12
#define INFRA_ERROR_STATE       -13
#define INFRA_ERROR_NOMEM       -14
#define INFRA_ERROR_UNKNOWN     -99

//-----------------------------------------------------------------------------
// Log Levels
//-----------------------------------------------------------------------------

#define INFRA_LOG_LEVEL_NONE  0
#define INFRA_LOG_LEVEL_ERROR 1
#define INFRA_LOG_LEVEL_WARN  2
#define INFRA_LOG_LEVEL_INFO  3
#define INFRA_LOG_LEVEL_DEBUG 4
#define INFRA_LOG_LEVEL_TRACE 5

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

typedef struct {
    struct {
        bool use_memory_pool;
        size_t pool_initial_size;
        size_t pool_alignment;
    } memory;
    
    struct {
        int level;
        size_t buffer_size;
        bool async_logging;
        const char* log_file;
    } log;
    
    struct {
        size_t hash_initial_size;
        uint32_t hash_load_factor;
        bool thread_safe;
    } ds;

    struct {
        uint32_t min_threads;      // 最小线程数
        uint32_t max_threads;      // 最大线程数
        uint32_t task_queue_size;  // 任务队列大小
        uint32_t task_timeout_ms;  // 任务超时时间
        struct {
            uint32_t io_threshold_us;   // IO任务判定阈值
            uint32_t cpu_threshold_us;  // CPU任务判定阈值
            uint32_t sample_window;     // 采样窗口大小
        } classify;
    } async;
} infra_config_t;

// 默认配置
extern const infra_config_t INFRA_DEFAULT_CONFIG;

// 配置相关函数
infra_error_t infra_config_init(infra_config_t* config);
infra_error_t infra_config_validate(const infra_config_t* config);
infra_error_t infra_config_apply(const infra_config_t* config);

//-----------------------------------------------------------------------------
// Initialization and Cleanup
//-----------------------------------------------------------------------------

typedef enum {
    INFRA_INIT_MEMORY = 1 << 0,
    INFRA_INIT_LOG    = 1 << 1,
    INFRA_INIT_DS     = 1 << 2,
    INFRA_INIT_ASYNC  = 1 << 3,
    INFRA_INIT_ALL    = 0xFFFFFFFF
} infra_init_flags_t;

infra_error_t infra_init_with_config(infra_init_flags_t flags, const infra_config_t* config);
infra_error_t infra_init(void);  // 使用默认配置
void infra_cleanup(void);
bool infra_is_initialized(infra_init_flags_t flag);

//-----------------------------------------------------------------------------
// Status and Health
//-----------------------------------------------------------------------------

typedef struct {
    bool initialized;                  // 是否已初始化
    infra_init_flags_t active_flags;   // 当前激活的模块
    struct {
        size_t current_usage;          // 当前内存使用量
        size_t peak_usage;            // 峰值内存使用量
        size_t total_allocations;      // 总分配次数
    } memory;
    struct {
        uint64_t log_entries;         // 日志条目数
        uint64_t dropped_entries;     // 丢弃的日志条目数
    } log;
} infra_status_t;

infra_error_t infra_get_status(infra_status_t* status);

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------

void* infra_malloc(size_t size);
void* infra_calloc(size_t nmemb, size_t size);
void* infra_realloc(void* ptr, size_t size);
void infra_free(void* ptr);
void* infra_memset(void* s, int c, size_t n);
void* infra_memcpy(void* dest, const void* src, size_t n);
void* infra_memmove(void* dest, const void* src, size_t n);
int infra_memcmp(const void* s1, const void* s2, size_t n);

//-----------------------------------------------------------------------------
// String Operations
//-----------------------------------------------------------------------------

size_t infra_strlen(const char* s);
char* infra_strcpy(char* dest, const char* src);
char* infra_strncpy(char* dest, const char* src, size_t n);
char* infra_strcat(char* dest, const char* src);
char* infra_strncat(char* dest, const char* src, size_t n);
int infra_strcmp(const char* s1, const char* s2);
int infra_strncmp(const char* s1, const char* s2, size_t n);
char* infra_strdup(const char* s);
char* infra_strndup(const char* s, size_t n);
char* infra_strchr(const char* s, int c);
char* infra_strrchr(const char* s, int c);
char* infra_strstr(const char* haystack, const char* needle);

//-----------------------------------------------------------------------------
// List Operations
//-----------------------------------------------------------------------------

typedef struct infra_list_node {
    struct infra_list_node* next;
    struct infra_list_node* prev;
    void* value;
} infra_list_node_t;

typedef struct {
    infra_list_node_t* head;
    infra_list_node_t* tail;
    size_t size;
} infra_list_t;

infra_error_t infra_list_create(infra_list_t** list);
void infra_list_destroy(infra_list_t* list);
infra_error_t infra_list_append(infra_list_t* list, void* value);
infra_error_t infra_list_remove(infra_list_t* list, infra_list_node_t* node);
infra_list_node_t* infra_list_head(infra_list_t* list);
infra_list_node_t* infra_list_node_next(infra_list_node_t* node);
void* infra_list_node_value(infra_list_node_t* node);

//-----------------------------------------------------------------------------
// Hash Operations
//-----------------------------------------------------------------------------

typedef struct infra_hash_node {
    char* key;
    void* value;
    struct infra_hash_node* next;
} infra_hash_node_t;

typedef struct {
    infra_hash_node_t** buckets;
    size_t size;
    size_t capacity;
} infra_hash_t;

infra_error_t infra_hash_create(infra_hash_t** hash, size_t capacity);
void infra_hash_destroy(infra_hash_t* hash);
infra_error_t infra_hash_put(infra_hash_t* hash, const char* key, void* value);
void* infra_hash_get(infra_hash_t* hash, const char* key);
void* infra_hash_remove(infra_hash_t* hash, const char* key);
void infra_hash_clear(infra_hash_t* hash);

//-----------------------------------------------------------------------------
// Red-Black Tree Operations
//-----------------------------------------------------------------------------

typedef enum {
    INFRA_RBTREE_RED,
    INFRA_RBTREE_BLACK
} infra_rbtree_color_t;

typedef struct infra_rbtree_node {
    int key;
    void* value;
    infra_rbtree_color_t color;
    struct infra_rbtree_node* parent;
    struct infra_rbtree_node* left;
    struct infra_rbtree_node* right;
} infra_rbtree_node_t;

typedef struct {
    infra_rbtree_node_t* root;
    size_t size;
} infra_rbtree_t;

infra_error_t infra_rbtree_create(infra_rbtree_t** tree);
void infra_rbtree_destroy(infra_rbtree_t* tree);
infra_error_t infra_rbtree_insert(infra_rbtree_t* tree, int key, void* value);
void* infra_rbtree_find(infra_rbtree_t* tree, int key);
void* infra_rbtree_remove(infra_rbtree_t* tree, int key);
void infra_rbtree_clear(infra_rbtree_t* tree);

//-----------------------------------------------------------------------------
// Buffer Operations
//-----------------------------------------------------------------------------

typedef struct {
    uint8_t* data;
    size_t size;
    size_t capacity;
} infra_buffer_t;

infra_error_t infra_buffer_init(infra_buffer_t* buf, size_t initial_capacity);
void infra_buffer_destroy(infra_buffer_t* buf);
infra_error_t infra_buffer_reserve(infra_buffer_t* buf, size_t capacity);
infra_error_t infra_buffer_write(infra_buffer_t* buf, const void* data, size_t size);
infra_error_t infra_buffer_read(infra_buffer_t* buf, void* data, size_t size);
size_t infra_buffer_readable(const infra_buffer_t* buf);
size_t infra_buffer_writable(const infra_buffer_t* buf);
void infra_buffer_reset(infra_buffer_t* buf);

//-----------------------------------------------------------------------------
// Logging
//-----------------------------------------------------------------------------

typedef void (*infra_log_callback_t)(int level, const char* file, int line, const char* func, const char* message);

void infra_log_set_level(int level);
void infra_log_set_callback(infra_log_callback_t callback);
void infra_log(int level, const char* file, int line, const char* func, const char* format, ...);

//-----------------------------------------------------------------------------
// Statistics
//-----------------------------------------------------------------------------

typedef struct {
    uint64_t total_operations;
    uint64_t successful_operations;
    uint64_t failed_operations;
    uint64_t total_bytes;
    uint64_t min_latency_us;
    uint64_t max_latency_us;
    uint64_t avg_latency_us;
    infra_error_t last_error;
    uint64_t last_error_time;
} infra_stats_t;

void infra_stats_init(infra_stats_t* stats);
void infra_stats_reset(infra_stats_t* stats);
void infra_stats_update(infra_stats_t* stats, bool success, uint64_t latency_us, size_t bytes, infra_error_t error);
void infra_stats_merge(infra_stats_t* dest, const infra_stats_t* src);
void infra_stats_print(const infra_stats_t* stats, const char* prefix);

//-----------------------------------------------------------------------------
// Time Management
//-----------------------------------------------------------------------------

infra_time_t infra_time_now(void);
infra_time_t infra_time_monotonic(void);
void infra_time_sleep(uint32_t ms);
void infra_time_yield(void);

#endif /* INFRA_H */
