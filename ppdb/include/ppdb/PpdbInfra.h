#ifndef PPDB_INFRA_H
#define PPDB_INFRA_H

#include "internal/infra/InfraCore.h"

typedef struct PpdbInfra PpdbInfra;

struct PpdbInfra {
    InfraCore *core;  // private implementation

    // public methods
    PpdbInfra* (*new)(void);  // constructor: ppdb_infra_new()
    void (*free)(PpdbInfra *self);  // destructor: ppdb_infra_free()
};

// Constructor
PpdbInfra* ppdb_infra_new(void);

// Destructor
void ppdb_infra_free(PpdbInfra *infra);

#endif // PPDB_INFRA_H