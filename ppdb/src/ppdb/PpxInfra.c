#include <stdlib.h>
#include "ppdb/PpxInfra.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

// Helper function to create a new error value
static InfraxError ppx_infra_new_error(infrax_error_t code, const char* message) {
    InfraxError error = {.code = code};
    if (message) {
        strncpy(error.message, message, sizeof(error.message) - 1);
    }
    error.message[sizeof(error.message) - 1] = '\0';  // Ensure null termination
    return error;
}

// Define a global singleton PpxInfra instance initialized only once.
static PpxInfra global_ppxInfra = {
    .core = NULL,
    .logger = NULL,
    .new = ppx_infra_new,
    .free = ppx_infra_free,
    .new_error = ppx_infra_new_error
};

// Private initialization function
static void ppx_infra_init(PpxInfra *self) {
    if (!self) return;
    
    // Initialize components
    self->core = get_global_infra_core();
    self->logger = get_global_infra_log();
    
    // Initialize methods
    self->new = ppx_infra_new;
    self->free = ppx_infra_free;
    self->new_error = ppx_infra_new_error;
}

// Constructor implementation
PpxInfra* ppx_infra_new(void) {
    PpxInfra* self = (PpxInfra*)malloc(sizeof(PpxInfra));
    if (self) {
        ppx_infra_init(self);
    }
    return self;
}

// Destructor implementation
void ppx_infra_free(PpxInfra* self) {
    if (!self) return;
    
    // Don't free global components
    if (self != &global_ppxInfra) {
        if (self->core) {
            self->core->free(self->core);
        }
        if (self->logger) {
            self->logger->free(self->logger);
        }
        free(self);
    }
}

// Public interface to retrieve the global PpxInfra instance.
PpxInfra* get_global_ppxInfra(void) {
    if (!global_ppxInfra.core) {
        ppx_infra_init(&global_ppxInfra);
    }
    return &global_ppxInfra;
}
