#include <cosmopolitan.h>

typedef enum {
    PPDB_LOG_DEBUG = 0,
    PPDB_LOG_INFO = 1,
    PPDB_LOG_WARN = 2,
    PPDB_LOG_ERROR = 3,
    PPDB_LOG_FATAL = 4
} ppdb_log_level_t;

typedef struct {
    FILE* fp;
    ppdb_log_level_t level;
    ppdb_sync_t* lock;
    bool console_output;
} ppdb_logger_t;

static ppdb_logger_t g_logger = {0};

static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

ppdb_error_t ppdb_log_init(const char* filename, ppdb_log_level_t level, bool console_output) {
    if (g_logger.fp) return PPDB_ERROR_ALREADY_INIT;
    
    if (filename) {
        g_logger.fp = fopen(filename, "a");
        if (!g_logger.fp) return PPDB_ERROR_IO;
    }
    
    ppdb_error_t err = ppdb_sync_create(&g_logger.lock, 0);  // mutex
    if (err != PPDB_OK) {
        if (g_logger.fp) fclose(g_logger.fp);
        return err;
    }
    
    g_logger.level = level;
    g_logger.console_output = console_output;
    return PPDB_OK;
}

void ppdb_log_close() {
    if (g_logger.fp) {
        fclose(g_logger.fp);
        g_logger.fp = NULL;
    }
    if (g_logger.lock) {
        ppdb_sync_destroy(g_logger.lock);
        g_logger.lock = NULL;
    }
}

static void log_write(ppdb_log_level_t level, const char* file, int line, const char* fmt, va_list args) {
    if (level < g_logger.level) return;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char msg[1024];
    vsnprintf(msg, sizeof(msg), fmt, args);
    
    char full_msg[2048];
    snprintf(full_msg, sizeof(full_msg), "[%s] [%s] %s:%d - %s\n",
             time_str, level_strings[level], file, line, msg);
    
    ppdb_sync_lock(g_logger.lock);
    
    if (g_logger.fp) {
        fputs(full_msg, g_logger.fp);
        fflush(g_logger.fp);
    }
    
    if (g_logger.console_output) {
        fputs(full_msg, stderr);
        fflush(stderr);
    }
    
    ppdb_sync_unlock(g_logger.lock);
}

#define PPDB_LOG(level, ...) \
    do { \
        va_list args; \
        va_start(args, fmt); \
        log_write(level, __FILE__, __LINE__, fmt, args); \
        va_end(args); \
    } while (0)

void ppdb_log_debug(const char* fmt, ...) {
    PPDB_LOG(PPDB_LOG_DEBUG, fmt);
}

void ppdb_log_info(const char* fmt, ...) {
    PPDB_LOG(PPDB_LOG_INFO, fmt);
}

void ppdb_log_warn(const char* fmt, ...) {
    PPDB_LOG(PPDB_LOG_WARN, fmt);
}

void ppdb_log_error(const char* fmt, ...) {
    PPDB_LOG(PPDB_LOG_ERROR, fmt);
}

void ppdb_log_fatal(const char* fmt, ...) {
    PPDB_LOG(PPDB_LOG_FATAL, fmt);
} 