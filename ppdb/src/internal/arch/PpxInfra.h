#ifndef PPX_INFRA_H
#define PPX_INFRA_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

// Forward declarations
typedef struct PpxInfra PpxInfra;
typedef struct PpxInfraClass PpxInfraClass;

// The "static" interface (like static methods in OOP)
struct PpxInfraClass {
    PpxInfra* (*new)(void);
    void (*free)(PpxInfra* self);
};

// The instance structure
struct PpxInfra {
    const PpxInfraClass* klass;  // 指向"类"方法表
    
    // Base components
    InfraxCore *core;   // Core functionality
    InfraxLog *logger;  // Logging functionality
};

// The "static" interface instance (like Java's Class object)
extern const PpxInfraClass PpxInfra_CLASS;

// Global instance
PpxInfra* get_global_ppxInfra(void);

#endif // PPX_INFRA_H