#ifndef PPDB_INFRA_H
#define PPDB_INFRA_H

#include "PpdbCore.h"

typedef struct PpdbInfra PpdbInfra;

struct PpdbInfra {
    InfraCore *core;  // private implementation

    // public methods
    PpdbInfra* (*new)(void);//constructor, ppdb_infra_new()
    void (*init)(PpdbInfra *self);//initializer, can be removed?
    void (*free)(PpdbInfra *self);//destructor, ppdb_infra_free()
    // int (*get_core_data)(const PpdbInfra *self);
    // void (*set_core_data)(PpdbInfra *self, int value);
};

// Constructor
PpdbInfra* ppdb_infra_new(void);

// Destructor
void ppdb_infra_free(PpdbInfra *infra);

#endif // PPDB_INFRA_H