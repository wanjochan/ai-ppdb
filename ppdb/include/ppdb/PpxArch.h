#ifndef PPX_ARCH_H
#define PPX_ARCH_H

#include "ppdb/PpxInfra.h"

// Forward declarations
typedef struct PpxPoly PpxPoly;
typedef struct PpxPeer PpxPeer;

// Main architecture component
typedef struct PpxArch {
    // Base
    PpxInfra *infra;  // Use pointer to avoid object slicing
    
    // Methods
    struct PpxArch* (*new)(void);
    void (*free)(struct PpxArch*);
} PpxArch;

// Constructor and destructor
PpxArch* ppx_arch_new(void);
void ppx_arch_free(PpxArch* self);

#endif // PPX_ARCH_H
