#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include <cosmopolitan.h>

//-----------------------------------------------------------------------------
// 日志系统实现
//-----------------------------------------------------------------------------

static ppdb_log_config_t g_log_config = {
    .enabled = true,
    .level = PPDB_LOG_DEBUG,
    .log_file = NULL,
    .outputs = 1  // 默认输出到标准错误
};

void ppdb_log_init(const ppdb_log_config_t* config) {
    if (config) {
        g_log_config = *config;
    }
}

void ppdb_log_cleanup(void) {
    // 目前无需清理
}

void ppdb_log(ppdb_log_level_t level, const char* fmt, ...) {
    if (!g_log_config.enabled || level < g_log_config.level) {
        return;
    }

    // 获取时间戳
    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    char timestamp[26];
    strftime(timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    // 获取日志级别字符串
    const char* level_str;
    switch (level) {
        case PPDB_LOG_DEBUG:
            level_str = "DEBUG";
            break;
        case PPDB_LOG_INFO:
            level_str = "INFO";
            break;
        case PPDB_LOG_WARN:
            level_str = "WARN";
            break;
        case PPDB_LOG_ERROR:
            level_str = "ERROR";
            break;
        case PPDB_LOG_FATAL:
            level_str = "FATAL";
            break;
        default:
            level_str = "UNKNOWN";
            break;
    }

    // 格式化消息
    va_list args;
    va_start(args, fmt);
    char message[1024];
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    // 输出日志
    if (g_log_config.outputs & 1) {  // 标准错误
        fprintf(stderr, "[%s] %s: %s\n", timestamp, level_str, message);
        fflush(stderr);
    }

    if (g_log_config.log_file && (g_log_config.outputs & 2)) {  // 文件
        FILE* fp = fopen(g_log_config.log_file, "a");
        if (fp) {
            fprintf(fp, "[%s] %s: %s\n", timestamp, level_str, message);
            fclose(fp);
        }
    }
}

void ppdb_debug(const char* fmt, ...) {
    if (!g_log_config.enabled || PPDB_LOG_DEBUG < g_log_config.level) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    char message[1024];
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    ppdb_log(PPDB_LOG_DEBUG, "%s", message);
} 