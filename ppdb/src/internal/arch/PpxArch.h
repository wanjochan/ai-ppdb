#ifndef PPX_ARCH_H
#define PPX_ARCH_H

#include "PpxInfra.h"

// Forward declarations
typedef struct PpxPoly PpxPoly;
typedef struct PpxPeer PpxPeer;
typedef struct PpxArch PpxArch;
typedef struct PpxArchClass PpxArchClass;

// The "static" interface (like static methods in OOP)
typedef struct PpxArchClass {
    PpxArch* (*new)(void);
    void (*free)(PpxArch* self);
} PpxArchClass;

// The instance structure
struct PpxArch {
    const PpxArchClass* klass;  // 指向"类"方法表
    
    // Base
    PpxInfra *infra;  // Use pointer to avoid object slicing
};

extern const PpxArchClass PpxArch_CLASS;

// Global instance
PpxArch* get_global_ppxArch(void);

#endif // PPX_ARCH_H
