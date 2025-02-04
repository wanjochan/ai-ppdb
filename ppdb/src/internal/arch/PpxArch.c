#include <stdlib.h>
#include "ppdb/PpxArch.h"
#include "ppdb/PpxInfra.h"

// Private functions
static void ppx_arch_init(PpxArch* self) {
    if (!self) return;
    
    // Initialize infra
    self->infra = ppx_infra_new();
    
    // Initialize methods
    self->new = ppx_arch_new;
    self->free = ppx_arch_free;
}

// Public functions
PpxArch* ppx_arch_new(void) {
    PpxArch* arch = (PpxArch*)malloc(sizeof(PpxArch));
    if (arch) {
        ppx_arch_init(arch);
    }
    return arch;
}

void ppx_arch_free(PpxArch* self) {
    if (!self) return;
    
    // Free infra
    if (self->infra) {
        self->infra->free(self->infra);
    }
    
    free(self);
}