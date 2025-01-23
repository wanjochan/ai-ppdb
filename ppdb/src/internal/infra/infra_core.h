/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 */

#ifndef INFRA_CORE_H
#define INFRA_CORE_H

#include "cosmopolitan.h"

#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_error.h"

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

typedef uint32_t infra_flags_t;
typedef uint64_t infra_time_t;
typedef uint64_t infra_handle_t;
typedef uint64_t INFRA_CORE_Handle_t;
typedef int infra_bool_t;

#define INFRA_TRUE  1
#define INFRA_FALSE 0

//-----------------------------------------------------------------------------
// Thread Types
//-----------------------------------------------------------------------------

typedef void* infra_mutex_t;
typedef void* infra_mutex_attr_t;
typedef void* infra_cond_t;
typedef void* infra_cond_attr_t;
typedef void* infra_thread_t;
typedef void* infra_thread_attr_t;
typedef void* (*infra_thread_func_t)(void*);

//-----------------------------------------------------------------------------
// Thread Operations
//-----------------------------------------------------------------------------

infra_error_t infra_mutex_create(infra_mutex_t* mutex);
void infra_mutex_destroy(infra_mutex_t mutex);
infra_error_t infra_mutex_lock(infra_mutex_t mutex);
infra_error_t infra_mutex_unlock(infra_mutex_t mutex);

infra_error_t infra_cond_init(infra_cond_t* cond);
void infra_cond_destroy(infra_cond_t cond);
infra_error_t infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex);
infra_error_t infra_cond_timedwait(infra_cond_t cond, infra_mutex_t mutex, uint32_t timeout_ms);
infra_error_t infra_cond_signal(infra_cond_t cond);
infra_error_t infra_cond_broadcast(infra_cond_t cond);

infra_error_t infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg);
infra_error_t infra_thread_join(infra_thread_t thread);

//TODO
//void infra_thread_exit(void* retval);

//TODO
//infra_thread_t infra_thread_self(void);

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
// Log Macros
//-----------------------------------------------------------------------------

#define INFRA_LOG_ERROR(fmt, ...) infra_log(INFRA_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define INFRA_LOG_WARN(fmt, ...)  infra_log(INFRA_LOG_LEVEL_WARN,  __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define INFRA_LOG_INFO(fmt, ...)  infra_log(INFRA_LOG_LEVEL_INFO,  __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define INFRA_LOG_DEBUG(fmt, ...) infra_log(INFRA_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define INFRA_LOG_TRACE(fmt, ...) infra_log(INFRA_LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------

// 网络配置标志
#define INFRA_CONFIG_FLAG_NONBLOCK (1 << 0)  // 非阻塞模式
#define INFRA_CONFIG_FLAG_NODELAY  (1 << 1)  // 禁用Nagle算法
#define INFRA_CONFIG_FLAG_KEEPALIVE (1 << 2) // 启用保活

typedef struct {
    infra_memory_config_t memory;
    
    struct {
        int level;
        const char* log_file;
    } log;
    
    struct {
        size_t hash_initial_size;
        uint32_t hash_load_factor;
    } ds;

    struct {
        size_t max_events;   // 单次处理的最大事件数
        bool edge_trigger;   // 是否启用边缘触发(仅epoll)
    } mux;

    struct {
        infra_flags_t flags;     // 网络配置标志
        uint32_t connect_timeout_ms;  // 连接超时时间
        uint32_t read_timeout_ms;     // 读取超时时间
        uint32_t write_timeout_ms;    // 写入超时时间
    } net;
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
    infra_memory_stats_t memory;      // 内存统计信息
    struct {
        uint64_t log_entries;         // 日志条目数
        uint64_t dropped_entries;     // 丢弃的日志条目数
    } log;
} infra_status_t;

infra_error_t infra_get_status(infra_status_t* status);

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
infra_error_t infra_list_init(infra_list_t* list);
void infra_list_cleanup(infra_list_t* list);
infra_error_t infra_list_append(infra_list_t* list, void* value);
infra_error_t infra_list_push_back(infra_list_t* list, void* value);
void* infra_list_pop_front(infra_list_t* list);
infra_error_t infra_list_remove(infra_list_t* list, infra_list_node_t* node);
bool infra_list_empty(const infra_list_t* list);
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
// Printf Operations
//-----------------------------------------------------------------------------

infra_error_t infra_printf(const char* format, ...);
infra_error_t infra_fprintf(FILE* stream, const char* format, ...);
int infra_vprintf(const char* format, va_list args);
int infra_vfprintf(FILE* stream, const char* format, va_list args);
int infra_snprintf(char* str, size_t size, const char* format, ...);
int infra_vsnprintf(char* str, size_t size, const char* format, va_list args);

const char* infra_error_string(int error_code);

//-----------------------------------------------------------------------------
// Ring Buffer Operations
//-----------------------------------------------------------------------------

typedef struct {
    uint8_t* buffer;
    size_t size;
    size_t read_pos;
    size_t write_pos;
    bool full;
} infra_ring_buffer_t;

infra_error_t infra_ring_buffer_init(infra_ring_buffer_t* rb, size_t size);
void infra_ring_buffer_destroy(infra_ring_buffer_t* rb);
infra_error_t infra_ring_buffer_write(infra_ring_buffer_t* rb, const void* data, size_t size);
infra_error_t infra_ring_buffer_read(infra_ring_buffer_t* rb, void* data, size_t size);
size_t infra_ring_buffer_readable(const infra_ring_buffer_t* rb);
size_t infra_ring_buffer_writable(const infra_ring_buffer_t* rb);
void infra_ring_buffer_reset(infra_ring_buffer_t* rb);

// 线程和进程
infra_error_t infra_sleep(uint32_t ms);
infra_error_t infra_yield(void);

// 时间操作
uint64_t infra_time_ms(void);

//-----------------------------------------------------------------------------
// Global State
//-----------------------------------------------------------------------------

// 全局状态结构
typedef struct {
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

    // 平台状态
    struct {
        bool is_windows;
    } platform;
} infra_global_t;

// 全局变量声明
extern infra_global_t g_infra;

//-----------------------------------------------------------------------------
// Random Number Operations
//-----------------------------------------------------------------------------

/**
 * @brief 设置随机数生成器的种子
 * @param seed 随机数种子
 */
void infra_random_seed(uint32_t seed);

// 获取当前工作目录
infra_error_t infra_get_cwd(char* buffer, size_t size);

#endif /* INFRA_CORE_H */
