#ifndef PPDB_INFRAX_LOG_H
#define PPDB_INFRAX_LOG_H

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

typedef struct InfraxLog {
    // Properties
    LogLevel min_log_level;  // Minimum log level to output
    
    // Methods
    struct InfraxLog* (*new)(void);
    void (*free)(struct InfraxLog*);
    void (*set_level)(struct InfraxLog*, LogLevel level);
    void (*debug)(struct InfraxLog*, const char* format, ...);
    void (*info)(struct InfraxLog*, const char* format, ...);
    void (*warn)(struct InfraxLog*, const char* format, ...);
    void (*error)(struct InfraxLog*, const char* format, ...);
} InfraxLog;

// Constructor and destructor
InfraxLog* infrax_log_new(void);
void infrax_log_free(InfraxLog* self);

// Public methods
void infrax_log_set_level(InfraxLog* self, LogLevel level);
void infrax_log_debug(InfraxLog* self, const char* format, ...);
void infrax_log_info(InfraxLog* self, const char* format, ...);
void infrax_log_warn(InfraxLog* self, const char* format, ...);
void infrax_log_error(InfraxLog* self, const char* format, ...);

InfraxLog* get_global_infra_log(void);

#endif // PPDB_INFRAX_LOG_H
