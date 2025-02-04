#include <stdio.h>
#include <stdlib.h>
#include "internal/infra/InfraCore.h"

// Private functions
static void infra_core_print(InfraCore *self) {
    if (!self) return;
    printf("InfraCore: data=%d\n", self->data);
}

static void infra_core_init(InfraCore *self) {
    if (!self) return;
    
    // Initialize data
    self->data = 0;
    
    // Initialize methods
    self->new = infra_core_new;
    self->free = infra_core_free;
    self->print = infra_core_print;
}

// Public functions
InfraCore* infra_core_new(void) {
    InfraCore *core = (InfraCore*)malloc(sizeof(InfraCore));
    if (core) {
        infra_core_init(core);
    }
    return core;
}

void infra_core_free(InfraCore *self) {
    if (!self) return;
    free(self);
} 