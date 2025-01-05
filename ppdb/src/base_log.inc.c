//-----------------------------------------------------------------------------
// 日志系统实现
//-----------------------------------------------------------------------------

static ppdb_log_config_t log_config = {0};
static ppdb_sync_t* log_sync = NULL;
static FILE* log_file = NULL;
static ppdb_log_level_t min_level = PPDB_LOG_INFO;

static const char* level_strings[] = {
    [PPDB_LOG_DEBUG] = "DEBUG",
    [PPDB_LOG_INFO] = "INFO",
    [PPDB_LOG_WARN] = "WARN",
    [PPDB_LOG_ERROR] = "ERROR",
    [PPDB_LOG_FATAL] = "FATAL"
};

void ppdb_log_init(const ppdb_log_config_t* config) {
    if (!config) return;

    // 创建同步对象
    if (!log_sync) {
        ppdb_sync_config_t sync_config = {
            .type = PPDB_SYNC_MUTEX,
            .use_lockfree = false,
            .enable_ref_count = false,
            .max_readers = 1,
            .backoff_us = 1,
            .max_retries = 100
        };
        ppdb_sync_create(&log_sync, &sync_config);
    }

    ppdb_sync_lock(log_sync);
    
    log_config = *config;
    min_level = config->level;

    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }

    if (config->log_file) {
        log_file = fopen(config->log_file, "a");
    }

    ppdb_sync_unlock(log_sync);
}

void ppdb_log_cleanup(void) {
    if (log_sync) {
        ppdb_sync_lock(log_sync);
        if (log_file) {
            fclose(log_file);
            log_file = NULL;
        }
        ppdb_sync_unlock(log_sync);
        ppdb_sync_destroy(log_sync);
        free(log_sync);
        log_sync = NULL;
    }
}

void ppdb_log(ppdb_log_level_t level, const char* fmt, ...) {
    if (!log_config.enabled || level < min_level) {
        return;
    }

    ppdb_sync_lock(log_sync);

    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    va_list args;
    va_start(args, fmt);

    if (log_file) {
        fprintf(log_file, "[%s] [%s] ", time_str, level_strings[level]);
        vfprintf(log_file, fmt, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    if (log_config.outputs & 1) {
        fprintf(stdout, "[%s] [%s] ", time_str, level_strings[level]);
        vfprintf(stdout, fmt, args);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    va_end(args);
    ppdb_sync_unlock(log_sync);
}

void ppdb_debug(const char* fmt, ...) {
    if (!log_config.enabled || PPDB_LOG_DEBUG < min_level) {
        return;
    }

    ppdb_sync_lock(log_sync);

    va_list args;
    va_start(args, fmt);

    if (log_file) {
        fprintf(log_file, "[DEBUG] ");
        vfprintf(log_file, fmt, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    if (log_config.outputs & 1) {
        fprintf(stdout, "[DEBUG] ");
        vfprintf(stdout, fmt, args);
        fprintf(stdout, "\n");
        fflush(stdout);
    }

    va_end(args);
    ppdb_sync_unlock(log_sync);
} 