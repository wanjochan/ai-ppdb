#include <stdlib.h>
#include "cosmopolitan.h"
#include "PpxInfra.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxLog.h"

// // Helper function to create a new error value
// static InfraxError ppx_infra_new_error(InfraxI32 code, const char* message) {
//     InfraxError error = {.code = code};
//     if (message) {
//         strncpy(error.message, message, sizeof(error.message) - 1);
//     }
//     error.message[sizeof(error.message) - 1] = '\0';  // Ensure null termination
//     return error;
// }

// Forward declaration of static variables
static PpxInfra global_ppxInfra;

// Private initialization function
static void ppx_infra_init(PpxInfra* self) {
    if (!self) return;
    
    // Initialize class pointer
    self->klass = &PpxInfra_CLASS;
    
    // Initialize components
    self->core = get_global_infrax_core();
    self->logger = get_global_infrax_log();
    //TODO if add new_memory?
}

// Constructor implementation
static PpxInfra* ppx_infra_new(void) {
    PpxInfra* self = (PpxInfra*)malloc(sizeof(PpxInfra));
    if (self) {
        ppx_infra_init(self);
    }
    return self;
}

// Destructor implementation
static void ppx_infra_free(PpxInfra* self) {
    if (!self) return;
    
    // Don't free global instance
    if (self != &global_ppxInfra) {
        // Don't free core and logger as they are global instances
        free(self);
    }
}

// The "static" interface implementation
const PpxInfraClass PpxInfra_CLASS = {
    .new = ppx_infra_new,
    .free = ppx_infra_free
};

// Global instance initialization
static PpxInfra global_ppxInfra = {
    .klass = &PpxInfra_CLASS,
    .core = NULL,
    .logger = NULL
};

// Get global instance
PpxInfra* get_global_ppxInfra(void) {
    if (!global_ppxInfra.core) {
        ppx_infra_init(&global_ppxInfra);
    }
    return &global_ppxInfra;
}
