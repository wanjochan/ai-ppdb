#ifndef PPDB_ARCH_H
#define PPDB_ARCH_H

// Forward declarations for main components
typedef struct PpdbInfra PpdbInfra;
typedef struct PpdbPoly PpdbPoly;
typedef struct PpdbPeer PpdbPeer;
typedef struct Ppdb Ppdb;

// Main PPDB structure
struct Ppdb {
    // Component instances (private)
    PpdbInfra *infra;  // Infrastructure component
    PpdbPoly *poly;    // Polymorphic component
    PpdbPeer *peer;    // Peer component

    // Public methods
    Ppdb* (*new)(void);      // constructor: ppdb_new()
    void (*free)(Ppdb *self);// destructor: ppdb_free()

    // Component access chain example:
    // self->infra->core->print(self->infra->core);

    // Future extensions
    // PpdbInfra* (*get_infra)(const Ppdb *self);
    // PpdbPoly* (*get_poly)(const Ppdb *self);
    // PpdbPeer* (*get_peer)(const Ppdb *self);
};

// Constructor and destructor
Ppdb* ppdb_new(void);
void ppdb_free(Ppdb *ppdb);

// Global instances (only if really necessary)
#ifdef PPDB_GLOBAL_INSTANCES
extern PpdbInfra* gInfra;
extern PpdbPoly* gPoly;
extern PpdbPeer* gPeer;
#endif

#endif // PPDB_ARCH_H




