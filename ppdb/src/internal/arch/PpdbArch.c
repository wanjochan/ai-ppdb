#include <stdlib.h>
#include "ppdb/PpdbArch.h"
#include "ppdb/PpdbInfra.h"

// Private functions
static void ppdb_init(Ppdb *self) {
    if (!self) return;
    
    // Initialize components
    self->infra = ppdb_infra_new();
    self->poly = NULL;  // TODO: implement when needed
    self->peer = NULL;  // TODO: implement when needed
    
    // Initialize methods
    self->new = ppdb_new;
    self->free = ppdb_free;
}

// Public functions
Ppdb* ppdb_new(void) {
    Ppdb *ppdb = (Ppdb*)malloc(sizeof(Ppdb));
    if (ppdb) {
        ppdb_init(ppdb);
    }
    return ppdb;
}

void ppdb_free(Ppdb *self) {
    if (!self) return;
    
    // Free components
    if (self->infra) {
        self->infra->free(self->infra);
    }
    if (self->poly) {
        // TODO: implement when needed
    }
    if (self->peer) {
        // TODO: implement when needed
    }
    
    free(self);
} 