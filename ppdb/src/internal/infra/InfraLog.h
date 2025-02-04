#ifndef PPDB_INFRA_LOG_H
#define PPDB_INFRA_LOG_H

#include <stdarg.h>

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

typedef struct InfraLog {
    // Properties
    LogLevel min_level;
    
    // Methods
    struct InfraLog* (*new)(void);
    void (*free)(struct InfraLog*);
    void (*set_level)(struct InfraLog*, LogLevel level);
    void (*debug)(struct InfraLog*, const char* format, ...);
    void (*info)(struct InfraLog*, const char* format, ...);
    void (*warn)(struct InfraLog*, const char* format, ...);
    void (*error)(struct InfraLog*, const char* format, ...);
} InfraLog;

// Constructor and destructor
InfraLog* infra_log_new(void);
void infra_log_free(InfraLog* self);

// Public methods
void infra_log_set_level(InfraLog* self, LogLevel level);
void infra_log_debug(InfraLog* self, const char* format, ...);
void infra_log_info(InfraLog* self, const char* format, ...);
void infra_log_warn(InfraLog* self, const char* format, ...);
void infra_log_error(InfraLog* self, const char* format, ...);

#endif // PPDB_INFRA_LOG_H
