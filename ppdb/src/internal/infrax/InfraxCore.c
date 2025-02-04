#include <stdio.h>
#include <stdlib.h>
#include "internal/infrax/InfraxCore.h"

// Private functions
static void infrax_core_print(InfraxCore *self) {
    if (!self) return;
    printf("InfraxCore: data=%d\n", self->data);
}

static void infrax_core_init(InfraxCore *self) {
    if (!self) return;
    
    // Initialize data
    self->data = 0;
    
    // Initialize methods
    self->new = infrax_core_new;
    self->free = infrax_core_free;
    self->print = infrax_core_print;
}

// Public functions
InfraxCore* infrax_core_new(void) {
    InfraxCore *core = (InfraxCore*)malloc(sizeof(InfraxCore));
    if (core) {
        infrax_core_init(core);
    }
    return core;
}

void infrax_core_free(InfraxCore *self) {
    if (!self) return;
    free(self);
}