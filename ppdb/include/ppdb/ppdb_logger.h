#ifndef PPDB_LOGGER_H
#define PPDB_LOGGER_H

#include <cosmopolitan.h>

// 日志级别定义
typedef enum {
    PPDB_LOG_DEBUG = 0,
    PPDB_LOG_INFO,
    PPDB_LOG_WARN,
    PPDB_LOG_ERROR,
    PPDB_LOG_FATAL
} ppdb_log_level_t;

// 日志输出类型
typedef enum {
    PPDB_LOG_CONSOLE = 1,  // 控制台输出
    PPDB_LOG_FILE = 2      // 文件输出
} ppdb_log_output_t;

// 日志类型
typedef enum {
    PPDB_LOG_TYPE_NONE = 0,
    PPDB_LOG_TYPE_ERROR = 1,
    PPDB_LOG_TYPE_WARN = 2,
    PPDB_LOG_TYPE_INFO = 4,
    PPDB_LOG_TYPE_DEBUG = 8,
    PPDB_LOG_TYPE_ALL = 15
} ppdb_log_type_t;

// 日志配置
typedef struct {
    bool enabled;              // 是否启用日志
    ppdb_log_output_t outputs; // 输出方式
    ppdb_log_type_t types;    // 日志类型
    bool async_mode;          // 是否异步
    size_t buffer_size;       // 缓冲区大小
    ppdb_log_level_t level;   // 日志级别
    const char* log_file;     // 日志文件路径
} ppdb_log_config_t;

// 日志初始化
void ppdb_log_init(const ppdb_log_config_t* config);

// 日志清理
void ppdb_log_cleanup(void);

// 设置日志级别
void ppdb_log_set_level(ppdb_log_level_t level);

// 获取当前日志级别
ppdb_log_level_t ppdb_logger_get_level(void);

// 启用/禁用日志
void ppdb_log_enable(bool enable);

// 设置日志输出方式
void ppdb_log_set_outputs(ppdb_log_output_t outputs);

// 设置日志类型
void ppdb_log_set_types(ppdb_log_type_t types);

// 日志记录函数
void ppdb_log(ppdb_log_level_t level, const char* file, int line, const char* fmt, ...);

// 日志宏定义，方便使用
#define PPDB_LOG_DEBUG(fmt, ...) ppdb_log(PPDB_LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PPDB_LOG_INFO(fmt, ...) ppdb_log(PPDB_LOG_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PPDB_LOG_WARN(fmt, ...) ppdb_log(PPDB_LOG_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PPDB_LOG_ERROR(fmt, ...) ppdb_log(PPDB_LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PPDB_LOG_FATAL(fmt, ...) ppdb_log(PPDB_LOG_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // PPDB_LOGGER_H 