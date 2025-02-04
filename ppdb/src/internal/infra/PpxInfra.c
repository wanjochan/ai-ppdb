#include <stdlib.h>
#include <stdarg.h>
#include "ppdb/PpxInfra.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

// Private initialization function
static void ppx_infra_init(PpxInfra *self) {
    if (!self) return;
    
    // Initialize components
    self->core = infrax_core_new();
    self->logger = infrax_log_new();
    
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
    
    // Free components
    if (self->core) {
        self->core->free(self->core);
    }
    if (self->logger) {
        self->logger->free(self->logger);
    }
    
    free(self);
}