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

// 日志输出类型
typedef enum {
    PPDB_LOG_CONSOLE = 1,
    PPDB_LOG_FILE = 2,
    PPDB_LOG_ALL = PPDB_LOG_CONSOLE | PPDB_LOG_FILE
} ppdb_log_output_t;

// 日志类型
typedef enum {
    PPDB_LOG_TYPE_SYSTEM = 1,
    PPDB_LOG_TYPE_STORAGE = 2,
    PPDB_LOG_TYPE_NETWORK = 4,
    PPDB_LOG_TYPE_ALL = 0xFF
} ppdb_log_type_t;

// 日志配置
typedef struct {
    bool enabled;
    ppdb_log_level_t level;
    ppdb_log_output_t outputs;
    ppdb_log_type_t types;
    const char* log_file;
    bool async_mode;
    size_t buffer_size;
} ppdb_log_config_t;

// 日志初始化和关闭
void ppdb_log_init(const ppdb_log_config_t* config);
void ppdb_log_shutdown(void);

// 日志控制
void ppdb_log_enable(bool enable);
void ppdb_log_set_outputs(ppdb_log_output_t outputs);
void ppdb_log_set_types(ppdb_log_type_t types);
void ppdb_log_set_level(ppdb_log_level_t level);

// 基础日志函数
void ppdb_log_debug(const char* fmt, ...);
void ppdb_log_info(const char* fmt, ...);
void ppdb_log_warn(const char* fmt, ...);
void ppdb_log_error(const char* fmt, ...);
void ppdb_log_fatal(const char* fmt, ...);

// 带类型的日志函数
void ppdb_log_debug_type(ppdb_log_type_t type, const char* fmt, ...);
void ppdb_log_info_type(ppdb_log_type_t type, const char* fmt, ...);
void ppdb_log_warn_type(ppdb_log_type_t type, const char* fmt, ...);
void ppdb_log_error_type(ppdb_log_type_t type, const char* fmt, ...);

// 工具函数
uint64_t now_us(void);

#endif // PPDB_KVSTORE_LOGGER_H 