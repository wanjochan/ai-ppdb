#ifndef PPDB_KVSTORE_LOGGER_H
#define PPDB_KVSTORE_LOGGER_H

#include <cosmopolitan.h>

// 日志级别
typedef enum {
    PPDB_LOG_DEBUG,
    PPDB_LOG_INFO,
    PPDB_LOG_WARN,
    PPDB_LOG_ERROR,
    PPDB_LOG_FATAL
} ppdb_log_level_t;

// 日志函数
void ppdb_log_debug(const char* fmt, ...);
void ppdb_log_info(const char* fmt, ...);
void ppdb_log_warn(const char* fmt, ...);
void ppdb_log_error(const char* fmt, ...);
void ppdb_log_fatal(const char* fmt, ...);

// 设置日志级别
void ppdb_log_set_level(ppdb_log_level_t level);

// 获取当前时间（微秒）
uint64_t now_us(void);

#endif // PPDB_KVSTORE_LOGGER_H 