#ifndef PPDB_INFRAX_CORE_H
#define PPDB_INFRAX_CORE_H

#include "internal/infrax/InfraxTypes.h"

// Forward declaration
typedef struct InfraxCore InfraxCore;

// Core structure definition
struct InfraxCore {
    // Private data
    int data;
    LogLevel min_log_level;  // Added for logging

    // Public methods
    struct InfraxCore* (*new)(void);     // constructor: infrax_core_new()
    void (*free)(struct InfraxCore *self);// destructor: infrax_core_free()
    void (*print)(struct InfraxCore *self);// print function (using libc printf)
    
    // Logging methods
    void (*set_log_level)(struct InfraxCore *self, LogLevel level);
    void (*log_message)(struct InfraxCore *self, LogLevel level, const char* format, ...);

    // Future extensions
    // int (*get_data)(const InfraxCore *self);
    // void (*set_data)(InfraxCore *self, int value);
};

// Constructor - creates and initializes a new instance
InfraxCore* infrax_core_new(void);

// Destructor - cleans up and frees the instance
void infrax_core_free(InfraxCore *self);

// Logging functions
void infrax_core_set_log_level(InfraxCore *self, LogLevel level);
void infrax_core_log_message(InfraxCore *self, LogLevel level, const char* format, ...);

InfraxCore* get_global_infra_core(void);

#endif // PPDB_INFRAX_CORE_H
