#ifndef PPDB_LOGGER_H
#define PPDB_LOGGER_H

#include <cosmopolitan.h>

// 日志级别
typedef enum {
    PPDB_LOG_DEBUG,
    PPDB_LOG_INFO,
    PPDB_LOG_WARN,
    PPDB_LOG_ERROR
} ppdb_log_level_t;

// 日志输出类型（位掩码）
typedef enum {
    PPDB_LOG_NONE     = 0,        // 禁用所有日志
    PPDB_LOG_CONSOLE  = 1 << 0,   // 输出到控制台
    PPDB_LOG_FILE     = 1 << 1,   // 输出到文件
    PPDB_LOG_ALL      = PPDB_LOG_CONSOLE | PPDB_LOG_FILE
} ppdb_log_output_t;

// 日志类型（位掩码）
typedef enum {
    PPDB_LOG_TYPE_NONE     = 0,        // 无
    PPDB_LOG_TYPE_SYSTEM   = 1 << 0,   // 系统日志
    PPDB_LOG_TYPE_MEMORY   = 1 << 1,   // 内存相关
    PPDB_LOG_TYPE_IO       = 1 << 2,   // IO相关
    PPDB_LOG_TYPE_PERF     = 1 << 3,   // 性能相关
    PPDB_LOG_TYPE_ALL      = 0xFFFF    // 所有类型
} ppdb_log_type_t;

// 日志配置
typedef struct {
    bool enabled;                    // 全局开关
    ppdb_log_output_t outputs;      // 输出方式
    ppdb_log_type_t types;          // 启用的日志类型
    bool async_mode;                 // 是否使用异步模式
    size_t buffer_size;             // 缓冲区大小
    const char* log_file;           // 日志文件路径
    ppdb_log_level_t level;         // 日志级别
} ppdb_log_config_t;

// 日志函数声明
void ppdb_log_init(const ppdb_log_config_t* config);
void ppdb_log_shutdown(void);
void ppdb_log_set_level(ppdb_log_level_t level);
void ppdb_log_enable(bool enable);
void ppdb_log_set_outputs(ppdb_log_output_t outputs);
void ppdb_log_set_types(ppdb_log_type_t types);

// 带类型的日志函数
void ppdb_log_debug_type(ppdb_log_type_t type, const char* format, ...);
void ppdb_log_info_type(ppdb_log_type_t type, const char* format, ...);
void ppdb_log_warn_type(ppdb_log_type_t type, const char* format, ...);
void ppdb_log_error_type(ppdb_log_type_t type, const char* format, ...);

// 兼容旧接口（默认为系统日志类型）
void ppdb_log_debug(const char* format, ...);
void ppdb_log_info(const char* format, ...);
void ppdb_log_warn(const char* format, ...);
void ppdb_log_error(const char* format, ...);

#endif // PPDB_LOGGER_H 