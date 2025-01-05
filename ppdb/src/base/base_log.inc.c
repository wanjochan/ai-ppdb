#ifndef PPDB_BASE_LOG_INC_C
#define PPDB_BASE_LOG_INC_C

// Log levels
#define PPDB_LOG_ERROR   0
#define PPDB_LOG_WARN    1
#define PPDB_LOG_INFO    2
#define PPDB_LOG_DEBUG   3
#define PPDB_LOG_TRACE   4

// Logger structure
typedef struct {
    FILE* fp;
    int level;
    ppdb_core_mutex_t* mutex;
} ppdb_logger_t;

static ppdb_logger_t g_logger = {0};

ppdb_error_t ppdb_log_init(const char* filename, int level) {
    if (g_logger.fp) return PPDB_ERR_EXISTS;

    // Create logger mutex
    ppdb_error_t err = ppdb_core_mutex_create(&g_logger.mutex);
    if (err != PPDB_OK) return err;

    if (filename) {
        g_logger.fp = fopen(filename, "a");
        if (!g_logger.fp) {
            ppdb_core_mutex_destroy(g_logger.mutex);
            return PPDB_ERR_INVALID_ARGUMENT;
        }
    } else {
        g_logger.fp = stdout;
    }

    g_logger.level = level;
    return PPDB_OK;
}

void ppdb_log_cleanup(void) {
    if (g_logger.mutex) {
        ppdb_core_mutex_destroy(g_logger.mutex);
        g_logger.mutex = NULL;
    }

    if (g_logger.fp && g_logger.fp != stdout) {
        fclose(g_logger.fp);
        g_logger.fp = NULL;
    }
}

static const char* log_level_str(int level) {
    switch (level) {
        case PPDB_LOG_ERROR: return "ERROR";
        case PPDB_LOG_WARN:  return "WARN ";
        case PPDB_LOG_INFO:  return "INFO ";
        case PPDB_LOG_DEBUG: return "DEBUG";
        case PPDB_LOG_TRACE: return "TRACE";
        default:             return "?????";
    }
}

void ppdb_log(int level, const char* fmt, ...) {
    if (!g_logger.fp || level > g_logger.level) return;

    ppdb_core_mutex_lock(g_logger.mutex);

    // Get current time
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // Print log header
    fprintf(g_logger.fp, "[%s][%s] ", time_str, log_level_str(level));

    // Print log message
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logger.fp, fmt, args);
    va_end(args);

    fprintf(g_logger.fp, "\n");
    fflush(g_logger.fp);

    ppdb_core_mutex_unlock(g_logger.mutex);
}

#endif // PPDB_BASE_LOG_INC_C 