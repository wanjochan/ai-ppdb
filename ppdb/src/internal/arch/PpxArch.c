#include "cosmopolitan.h"
#include "PpxArch.h"
#include "PpxInfra.h"

// Forward declaration of static variables
static PpxArch global_ppxArch;

// Private initialization function
static void ppx_arch_init(PpxArch* self) {
    if (!self) return;
    
    // Initialize infra
    self->infra = get_global_ppxInfra();
    
    // Initialize class pointer
    self->klass = &PpxArch_CLASS;
}

// Constructor implementation
PpxArch* ppx_arch_new(void) {
    PpxArch* self = (PpxArch*)malloc(sizeof(PpxArch));
    if (self) {
        ppx_arch_init(self);
    }
    return self;
}

// Destructor implementation
void ppx_arch_free(PpxArch* self) {
    if (!self) return;
    
    // Don't free global instance
    if (self != &global_ppxArch) {
        // Don't free infra as it's a global instance
        free(self);
    }
}

// The "static" interface implementation
const PpxArchClass PpxArch_CLASS = {
    .new = ppx_arch_new,
    .free = ppx_arch_free
};

// Global instance initialization
static PpxArch global_ppxArch = {
    .klass = &PpxArch_CLASS,
    .infra = NULL
};

// Get global instance
PpxArch* get_global_ppxArch(void) {
    if (!global_ppxArch.infra) {
        ppx_arch_init(&global_ppxArch);
    }
    return &global_ppxArch;
}
