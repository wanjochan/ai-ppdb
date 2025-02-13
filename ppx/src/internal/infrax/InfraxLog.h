#ifndef PPDB_INFRAX_LOG_H
#define PPDB_INFRAX_LOG_H

//design pattern: singleton

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

// Forward declarations
typedef struct InfraxLog InfraxLog;
typedef struct InfraxLogClassType InfraxLogClassType;

// The "static" interface (like static methods in OOP)
struct InfraxLogClassType {
    InfraxLog* (*singleton)(void);
};

// The instance structure
struct InfraxLog {
    InfraxLog* self;
    InfraxLogClassType* klass;//InfraxLogClass

    // Properties
    LogLevel min_log_level;  // Minimum log level to output
    
    // Instance methods
    void (*set_level)(InfraxLog* self, LogLevel level);
    void (*debug)(InfraxLog* self, const char* format, ...);
    void (*info)(InfraxLog* self, const char* format, ...);
    void (*warn)(InfraxLog* self, const char* format, ...);
    void (*error)(InfraxLog* self, const char* format, ...);
};

// The "static" interface instance (like Java's Class object)
extern InfraxLogClassType InfraxLogClass;

#endif // PPDB_INFRAX_LOG_H
