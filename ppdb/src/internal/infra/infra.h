/*
 * @cursor:protected
 * This file is considered semi-read-only by Cursor AI.
 * Any modifications should be discussed and confirmed before applying.
 *
 * infra.h - Infrastructure Layer
 */

#ifndef INFRA_H
#define INFRA_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>

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
// Error Codes
//-----------------------------------------------------------------------------

#define INFRA_OK               0
#define INFRA_ERROR_GENERIC   -1
#define INFRA_ERROR_MEMORY    -2
#define INFRA_ERROR_IO        -3
#define INFRA_ERROR_TIMEOUT   -4
#define INFRA_ERROR_BUSY      -5
#define INFRA_ERROR_AGAIN     -6
#define INFRA_ERROR_INVALID   -7
#define INFRA_ERROR_NOTFOUND  -8
#define INFRA_ERROR_EXISTS    -9
#define INFRA_ERROR_FULL      -10
#define INFRA_ERROR_EMPTY     -11
#define INFRA_ERROR_OVERFLOW  -12
#define INFRA_ERROR_UNDERFLOW -13
#define INFRA_ERROR_SYSTEM    -14
#define INFRA_ERROR_PROTOCOL  -15
#define INFRA_ERROR_NETWORK   -16
#define INFRA_ERROR_SECURITY  -17

const char* infra_error_string(infra_error_t error);

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
// Buffer Operations
//-----------------------------------------------------------------------------

typedef struct infra_buffer {
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

#define INFRA_LOG_LEVEL_NONE    0
#define INFRA_LOG_LEVEL_ERROR   1
#define INFRA_LOG_LEVEL_WARN    2
#define INFRA_LOG_LEVEL_INFO    3
#define INFRA_LOG_LEVEL_DEBUG   4
#define INFRA_LOG_LEVEL_TRACE   5

typedef void (*infra_log_callback_t)(int level, const char* file, int line,
                                   const char* func, const char* msg);

void infra_log_set_level(int level);
void infra_log_set_callback(infra_log_callback_t callback);
void infra_log(int level, const char* file, int line, const char* func,
               const char* format, ...);

#define INFRA_LOG_ERROR(fmt, ...) \
    infra_log(INFRA_LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define INFRA_LOG_WARN(fmt, ...) \
    infra_log(INFRA_LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define INFRA_LOG_INFO(fmt, ...) \
    infra_log(INFRA_LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define INFRA_LOG_DEBUG(fmt, ...) \
    infra_log(INFRA_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define INFRA_LOG_TRACE(fmt, ...) \
    infra_log(INFRA_LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

//-----------------------------------------------------------------------------
// Statistics
//-----------------------------------------------------------------------------

typedef struct infra_stats {
    uint64_t total_operations;
    uint64_t successful_operations;
    uint64_t failed_operations;
    uint64_t total_bytes;
    uint64_t min_latency_us;
    uint64_t max_latency_us;
    uint64_t avg_latency_us;
    uint64_t last_error_time;
    infra_error_t last_error;
} infra_stats_t;

void infra_stats_init(infra_stats_t* stats);
void infra_stats_reset(infra_stats_t* stats);
void infra_stats_update(infra_stats_t* stats, bool success, uint64_t latency_us,
                       size_t bytes, infra_error_t error);
void infra_stats_merge(infra_stats_t* dest, const infra_stats_t* src);
void infra_stats_print(const infra_stats_t* stats, const char* prefix);

#endif /* INFRA_H */
