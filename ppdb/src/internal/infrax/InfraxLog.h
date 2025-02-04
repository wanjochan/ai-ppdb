#ifndef PPDB_INFRAX_LOG_H
#define PPDB_INFRAX_LOG_H

#include <stdarg.h>
#include "internal/infrax/InfraxTypes.h"
#include "internal/infrax/InfraxCore.h"

typedef struct InfraxLog {
    // Properties
    InfraxCore* core;  // Reference to InfraxCore instance
    
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
