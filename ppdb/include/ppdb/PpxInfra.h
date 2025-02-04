#ifndef PPX_INFRA_H
#define PPX_INFRA_H

#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

typedef struct PpxInfra {
    // Base components
    InfraxCore *core;   // Core functionality
    InfraxLog *logger;  // Logging functionality
    
    // Methods
    struct PpxInfra* (*new)(void);
    void (*free)(struct PpxInfra*);
} PpxInfra;

// Constructor and destructor
PpxInfra* ppx_infra_new(void);
void ppx_infra_free(PpxInfra* self);

#endif // PPX_INFRA_H