#ifndef PPDB_BASE_LOG_INC_C
#define PPDB_BASE_LOG_INC_C

// Log context
static struct {
    FILE* file;
    int level;
    bool thread_safe;
    ppdb_engine_mutex_t* mutex;
} g_log_ctx = {NULL, PPDB_LOG_INFO, false, NULL};

// Level strings
static const char* g_level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

// Internal logging function
static void log_write(int level, const char* format, va_list args) {
    if (!g_log_ctx.file || level < g_log_ctx.level) return;

    // Get current time
    time_t now;
    time(&now);
    struct tm* tm = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    // Lock if thread safe
    if (g_log_ctx.thread_safe && g_log_ctx.mutex) {
        ppdb_engine_mutex_lock(g_log_ctx.mutex);
    }

    // Write log entry
    fprintf(g_log_ctx.file, "[%s] [%s] ", time_str, g_level_strings[level]);
    vfprintf(g_log_ctx.file, format, args);
    fprintf(g_log_ctx.file, "\n");
    fflush(g_log_ctx.file);

    // Unlock if thread safe
    if (g_log_ctx.thread_safe && g_log_ctx.mutex) {
        ppdb_engine_mutex_unlock(g_log_ctx.mutex);
    }
}

ppdb_error_t ppdb_log_init(const char* filename, int level, bool thread_safe) {
    if (!filename) return PPDB_ERR_NULL_POINTER;
    if (level < PPDB_LOG_DEBUG || level > PPDB_LOG_ERROR) return PPDB_ERR_INVALID_ARGUMENT;
    if (g_log_ctx.file) return PPDB_ERR_EXISTS;

    // Open log file
    g_log_ctx.file = fopen(filename, "a");
    if (!g_log_ctx.file) return PPDB_ERR_INVALID_STATE;

    // Set log level
    g_log_ctx.level = level;
    g_log_ctx.thread_safe = thread_safe;

    // Create mutex if thread safe
    if (thread_safe) {
        ppdb_error_t err = ppdb_engine_mutex_create(&g_log_ctx.mutex);
        if (err != PPDB_OK) {
            fclose(g_log_ctx.file);
            g_log_ctx.file = NULL;
            return err;
        }
    }

    return PPDB_OK;
}

void ppdb_log_close(void) {
    if (g_log_ctx.file) {
        fclose(g_log_ctx.file);
        g_log_ctx.file = NULL;
    }

    if (g_log_ctx.mutex) {
        ppdb_engine_mutex_destroy(g_log_ctx.mutex);
        g_log_ctx.mutex = NULL;
    }

    g_log_ctx.level = PPDB_LOG_INFO;
    g_log_ctx.thread_safe = false;
}

void ppdb_log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_write(PPDB_LOG_DEBUG, format, args);
    va_end(args);
}

void ppdb_log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_write(PPDB_LOG_INFO, format, args);
    va_end(args);
}

void ppdb_log_warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_write(PPDB_LOG_WARN, format, args);
    va_end(args);
}

void ppdb_log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_write(PPDB_LOG_ERROR, format, args);
    va_end(args);
}

#endif // PPDB_BASE_LOG_INC_C 