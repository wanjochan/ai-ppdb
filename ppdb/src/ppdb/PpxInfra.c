#include <stdlib.h>
#include "ppdb/PpxInfra.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"
#include "internal/infrax/InfraxError.h"

// Define a global singleton PpxInfra instance initialized only once.
// 全局对象只在启动时初始化，后续只需引用该对象，避免重复赋值 overhead.
static PpxInfra global_ppxInfra = {
    .core = NULL,
    .logger = NULL,
    .error = NULL
};

// Private initialization function
static void ppx_infra_init(PpxInfra *self) {
    if (!self) return;
    
    // Initialize components
    self->core = infrax_core_new();
    self->logger = infrax_log_new();
    self->error = infrax_error_new();
    
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
    if (self->error) {
        infrax_error_free(self->error);
    }
    
    free(self);
}

// Public interface to retrieve the global PpxInfra instance.
PpxInfra* get_global_ppxInfra(void) {
    if (!global_ppxInfra.core) {
        global_ppxInfra.core = get_global_infra_core();
        global_ppxInfra.logger = get_global_infra_log();
        global_ppxInfra.error = get_global_infrax_error();
    }
    return &global_ppxInfra;
}
