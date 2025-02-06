#ifndef PPDB_INFRAX_LOG_H
#define PPDB_INFRAX_LOG_H

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

// Forward declarations
typedef struct InfraxLog InfraxLog;
typedef struct InfraxLogClass InfraxLogClass;

// The "static" interface (like static methods in OOP)
struct InfraxLogClass {
    InfraxLog* (*new)(void);
    void (*free)(InfraxLog* self);
};

// The instance structure
struct InfraxLog {
    const InfraxLogClass* klass;  // 指向"类"方法表
    
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
extern const InfraxLogClass InfraxLog_CLASS;

// Global instance
InfraxLog* get_global_infrax_log(void);

#endif // PPDB_INFRAX_LOG_H
