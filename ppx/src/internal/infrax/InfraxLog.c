#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include "internal/infrax/InfraxLog.h"

// Forward declaration of static variables
static InfraxLog global_infra_log;

// Private functions
static void get_time_str(char* buffer, size_t size) {
    time_t now;
    struct tm* timeinfo;
    
    time(&now);
    timeinfo = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", timeinfo);
}

static const char* level_to_str(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

static void log_message(InfraxLog* self, LogLevel level, const char* format, va_list args) {
    if (!self || !format) return;
    
    // Check log level
    if (level < self->min_log_level) return;
    
    char time_str[32];
    get_time_str(time_str, sizeof(time_str));
    
    // First format the message with the timestamp and level
    char msg_buffer[1024];  // Reasonable buffer size
    int prefix_len = snprintf(msg_buffer, sizeof(msg_buffer), "[%s][%s] ", 
                            time_str, level_to_str(level));
    if (prefix_len < 0 || prefix_len >= sizeof(msg_buffer)) return;
    
    // Then format the actual message
    vsnprintf(msg_buffer + prefix_len, sizeof(msg_buffer) - prefix_len, format, args);
    
    // Output the message
    fprintf(stderr, "%s\n", msg_buffer);
}

// Instance methods
static void infrax_log_set_level(InfraxLog* self, LogLevel level) {
    if (!self) return;
    self->min_log_level = level;
}

static void infrax_log_debug(InfraxLog* self, const char* format, ...) {
    if (!self) return;
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

static void infrax_log_info(InfraxLog* self, const char* format, ...) {
    if (!self) return;
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_INFO, format, args);
    va_end(args);
}

static void infrax_log_warn(InfraxLog* self, const char* format, ...) {
    if (!self) return;
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_WARN, format, args);
    va_end(args);
}

static void infrax_log_error(InfraxLog* self, const char* format, ...) {
    if (!self) return;
    va_list args;
    va_start(args, format);
    log_message(self, LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

// Private initialization function
static void infrax_log_init(InfraxLog* self) {
    if (!self) return;
    
    // Initialize class pointer
    self->klass = &InfraxLog_CLASS;
    
    // Initialize data
    self->min_log_level = LOG_LEVEL_INFO;  // Default log level
    
    // Initialize instance methods
    self->set_level = infrax_log_set_level;
    self->debug = infrax_log_debug;
    self->info = infrax_log_info;
    self->warn = infrax_log_warn;
    self->error = infrax_log_error;
}

// Constructor implementation
static InfraxLog* infrax_log_new(void) {
    InfraxLog* self = (InfraxLog*)malloc(sizeof(InfraxLog));
    if (self) {
        infrax_log_init(self);
    }
    return self;
}

// Destructor implementation
static void infrax_log_free(InfraxLog* self) {
    if (!self) return;
    
    // Don't free global instance
    if (self != &global_infra_log) {
        free(self);
    }
}

// The "static" interface implementation
const InfraxLogClass InfraxLog_CLASS = {
    .new = infrax_log_new,
    .free = infrax_log_free
};

// Global instance initialization
static InfraxLog global_infrax_log = {
    .klass = &InfraxLog_CLASS,
    .min_log_level = LOG_LEVEL_INFO,
    .set_level = infrax_log_set_level,
    .debug = infrax_log_debug,
    .info = infrax_log_info,
    .warn = infrax_log_warn,
    .error = infrax_log_error
};

// Get global instance
InfraxLog* get_global_infrax_log(void) {
    if (!global_infrax_log.klass) {
        infrax_log_init(&global_infrax_log);
    }
    return &global_infrax_log;
}