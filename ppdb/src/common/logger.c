#include "ppdb/logger.h"
#include <cosmopolitan.h>

static ppdb_log_level_t current_level = PPDB_LOG_INFO;
static bool is_enabled = true;
static ppdb_log_output_t current_outputs = PPDB_LOG_CONSOLE;
static ppdb_log_type_t current_types = PPDB_LOG_TYPE_ALL;
static FILE* log_file = NULL;
static bool async_mode = false;
static size_t buffer_size = 4096;

void ppdb_log_init(const ppdb_log_config_t* config) {
    if (config) {
        is_enabled = config->enabled;
        current_outputs = config->outputs;
        current_types = config->types;
        async_mode = config->async_mode;
        buffer_size = config->buffer_size;
        current_level = config->level;
        if ((config->outputs & PPDB_LOG_FILE) && config->log_file) {
            log_file = fopen(config->log_file, "a");
        }
    }
}

void ppdb_log_shutdown(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void ppdb_log_set_level(ppdb_log_level_t level) { current_level = level; }
void ppdb_log_enable(bool enable) { is_enabled = enable; }
void ppdb_log_set_outputs(ppdb_log_output_t outputs) { current_outputs = outputs; }
void ppdb_log_set_types(ppdb_log_type_t types) { current_types = types; }

static inline void log_output(const char* format, va_list args) {
    if (current_outputs & PPDB_LOG_CONSOLE) {
        vprintf(format, args);
        printf("\n");
    }
    if ((current_outputs & PPDB_LOG_FILE) && log_file) {
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}

#define DEFINE_LOG_FUNC(name, level) \
void ppdb_log_##name(const char* format, ...) { \
    if (!is_enabled || current_level > PPDB_LOG_##level) return; \
    va_list args; \
    va_start(args, format); \
    log_output(format, args); \
    va_end(args); \
}

#define DEFINE_LOG_TYPE_FUNC(name, level) \
void ppdb_log_##name##_type(ppdb_log_type_t type, const char* format, ...) { \
    if (!is_enabled || current_level > PPDB_LOG_##level || !(current_types & type)) return; \
    va_list args; \
    va_start(args, format); \
    log_output(format, args); \
    va_end(args); \
}

DEFINE_LOG_FUNC(debug, DEBUG)
DEFINE_LOG_FUNC(info, INFO)
DEFINE_LOG_FUNC(warn, WARN)
DEFINE_LOG_FUNC(error, ERROR)

DEFINE_LOG_TYPE_FUNC(debug, DEBUG)
DEFINE_LOG_TYPE_FUNC(info, INFO)
DEFINE_LOG_TYPE_FUNC(warn, WARN)
DEFINE_LOG_TYPE_FUNC(error, ERROR) 