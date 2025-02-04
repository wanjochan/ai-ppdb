#include <stdlib.h>
#include "ppdb/PpxInfra.h"
#include "internal/infrax/InfraxCore.h"

// Private functions
static void ppx_infra_init(PpxInfra *self) {
    if (!self) return;
    
    // Initialize core
    self->core = infrax_core_new();
    
    // Initialize methods
    self->new = ppx_infra_new;
    self->free = ppx_infra_free;
}

// Public functions
PpxInfra* ppx_infra_new(void) {
    PpxInfra *infra = (PpxInfra*)malloc(sizeof(PpxInfra));
    if (infra) {
        ppx_infra_init(infra);
    }
    return infra;
}

void ppx_infra_free(PpxInfra *self) {
    if (!self) return;
    
    // Free core
    if (self->core) {
        self->core->free(self->core);
    }
    
    free(self);
}