#ifndef PPX_INFRA_H
#define PPX_INFRA_H

#include "internal/infrax/InfraxCore.h"

typedef struct PpxInfra {
    // Base
    InfraxCore *core;  // Use pointer to avoid object slicing
    
    // Methods
    struct PpxInfra* (*new)(void);
    void (*free)(struct PpxInfra*);
} PpxInfra;

// Constructor and destructor
PpxInfra* ppx_infra_new(void);
void ppx_infra_free(PpxInfra* self);

#endif // PPX_INFRA_H