#include <stdlib.h>
#include "ppdb/PpdbInfra.h"
#include "internal/infra/InfraCore.h"

// Private functions
static void ppdb_infra_init(PpdbInfra *self) {
    if (!self) return;
    
    // Initialize core
    self->core = infra_core_new();
    
    // Initialize methods
    self->new = ppdb_infra_new;
    self->free = ppdb_infra_free;
}

// Public functions
PpdbInfra* ppdb_infra_new(void) {
    PpdbInfra *infra = (PpdbInfra*)malloc(sizeof(PpdbInfra));
    if (infra) {
        ppdb_infra_init(infra);
    }
    return infra;
}

void ppdb_infra_free(PpdbInfra *self) {
    if (!self) return;
    
    // Free core
    if (self->core) {
        self->core->free(self->core);
    }
    
    free(self);
} 